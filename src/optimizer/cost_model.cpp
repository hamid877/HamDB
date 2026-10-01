#include "optimizer/cost_model.hpp"
#include "planner/nested_loop_join_plan.hpp"
#include "planner/hash_join_plan.hpp"
#include "planner/top_k_plan.hpp"
#include "executor/constant_expression.hpp"
#include <algorithm>
#include <cmath>

namespace hamdb::optimizer {

/// Fraction of full table rows assumed for an IndexScan without statistics.
static constexpr double kIndexScanFraction = 0.1;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

CostModel::CostModel(StatsLookup stats)
    : stats_(std::move(stats))
{
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

double CostModel::rowsForScan(const std::string& table_name) const
{
    if (stats_) {
        TableStatistics ts = stats_(table_name);
        if (ts.row_count > 0) {
            return static_cast<double>(ts.row_count);
        }
    }
    return kDefaultRows;
}

// ---------------------------------------------------------------------------
// estimateRows — output cardinality for each logical node type
// ---------------------------------------------------------------------------

double CostModel::estimateRows(const planner::LogicalPlanNode& node) const
{
    using T = planner::LogicalPlanType;

    switch (node.getType()) {
    // ----------------------------------------------------------------
    // Leaf nodes
    // ----------------------------------------------------------------
    case T::SEQ_SCAN: {
        const auto& scan =
            dynamic_cast<const planner::SeqScanPlanNode&>(node);
        return rowsForScan(scan.getTableName());
    }
    case T::INDEX_SCAN: {
        // No index-cost knowledge; fall back to a fraction of the table.
        return kDefaultRows * 0.1;
    }
    case T::VALUES:
        // VALUES rows are determined at parse time; we don't have the count
        // here without evaluating expressions, so return a conservative 1.
        return 1.0;

    // ----------------------------------------------------------------
    // Unary operators — pass-through with optional shrinkage
    // ----------------------------------------------------------------
    case T::FILTER: {
        double child =
            node.getChildren().empty()
                ? kDefaultRows
                : estimateRows(*node.getChildren()[0]);
        // Default selectivity 1/3 when no predicate stats available.
        return std::max(1.0, child * 0.33);
    }
    case T::PROJECTION:
    case T::SORT:
    case T::ORDER_BY:
    case T::HAVING:
    case T::AGGREGATION:
        return node.getChildren().empty()
                   ? kDefaultRows
                   : estimateRows(*node.getChildren()[0]);

    case T::LIMIT: {
        const auto& lim =
            dynamic_cast<const planner::LimitPlanNode&>(node);
        double child =
            node.getChildren().empty()
                ? kDefaultRows
                : estimateRows(*node.getChildren()[0]);
        // Evaluate the limit constant if possible.
        if (const auto* ce =
                dynamic_cast<const ConstantExpression*>(lim.getLimit())) {
            try {
                Tuple empty;
                Schema empty_schema;
                Value v = ce->evaluate(empty, empty_schema);
                if (v.getType() == TypeId::Integer) {
                    double k = static_cast<double>(v.getAsInteger());
                    return std::min(child, k);
                }
            } catch (...) {
                // Ignore evaluation errors; use fallback value.
            }
        }
        return std::min(child, kDefaultK);
    }

    case T::TOP_K: {
        const auto& topk =
            dynamic_cast<const planner::LogicalTopKNode&>(node);
        double child =
            node.getChildren().empty()
                ? kDefaultRows
                : estimateRows(*node.getChildren()[0]);
        if (const auto* ce =
                dynamic_cast<const ConstantExpression*>(topk.getLimit())) {
            try {
                Tuple empty;
                Schema empty_schema;
                Value v = ce->evaluate(empty, empty_schema);
                if (v.getType() == TypeId::Integer) {
                    double k = static_cast<double>(v.getAsInteger());
                    return std::min(child, k);
                }
            } catch (...) {
                // Ignore evaluation errors; use fallback value.
            }
        }
        return std::min(child, kDefaultK);
    }

    // ----------------------------------------------------------------
    // Binary join nodes
    // ----------------------------------------------------------------
    case T::NESTED_LOOP_JOIN:
    case T::HASH_JOIN: {
        if (node.getChildren().size() < 2) {
            return kDefaultRows;
        }
        double left  = estimateRows(*node.getChildren()[0]);
        double right = estimateRows(*node.getChildren()[1]);
        // Rough equi-join output: min(left, right) rows on average.
        return std::max(1.0, std::min(left, right));
    }

    // ----------------------------------------------------------------
    // DML — no rows emitted
    // ----------------------------------------------------------------
    case T::INSERT:
    case T::UPDATE:
    case T::DELETE:
        return 0.0;
    }

    return kDefaultRows; // unreachable but keeps compiler happy
}

// ---------------------------------------------------------------------------
// estimateCost — recursive operator cost
// ---------------------------------------------------------------------------

double CostModel::estimateCost(const planner::LogicalPlanNode& node) const
{
    using T = planner::LogicalPlanType;

    switch (node.getType()) {
    // ----------------------------------------------------------------
    // SeqScan  ≈  row_count
    // ----------------------------------------------------------------
    case T::SEQ_SCAN: {
        const auto& scan =
            dynamic_cast<const planner::SeqScanPlanNode&>(node);
        return rowsForScan(scan.getTableName());
    }

    // ----------------------------------------------------------------
    // IndexScan  — cheaper than SeqScan (fraction)
    // ----------------------------------------------------------------
    case T::INDEX_SCAN:
        return kDefaultRows * kIndexScanFraction;

    // ----------------------------------------------------------------
    // Filter  ≈  child_cost + child_rows
    // ----------------------------------------------------------------
    case T::FILTER: {
        if (node.getChildren().empty()) {
            return kDefaultRows;
        }
        double child_cost = estimateCost(*node.getChildren()[0]);
        double child_rows = estimateRows(*node.getChildren()[0]);
        return child_cost + child_rows;
    }

    // ----------------------------------------------------------------
    // NestedLoopJoin  ≈  left_rows × right_rows + child costs
    // ----------------------------------------------------------------
    case T::NESTED_LOOP_JOIN: {
        if (node.getChildren().size() < 2) {
            return kDefaultRows * kDefaultRows;
        }
        double left_rows  = estimateRows(*node.getChildren()[0]);
        double right_rows = estimateRows(*node.getChildren()[1]);
        double left_cost  = estimateCost(*node.getChildren()[0]);
        double right_cost = estimateCost(*node.getChildren()[1]);
        return left_rows * right_rows + left_cost + right_cost;
    }

    // ----------------------------------------------------------------
    // HashJoin  ≈  left_rows + right_rows + child costs
    // ----------------------------------------------------------------
    case T::HASH_JOIN: {
        if (node.getChildren().size() < 2) {
            return kDefaultRows + kDefaultRows;
        }
        double left_rows  = estimateRows(*node.getChildren()[0]);
        double right_rows = estimateRows(*node.getChildren()[1]);
        double left_cost  = estimateCost(*node.getChildren()[0]);
        double right_cost = estimateCost(*node.getChildren()[1]);
        return left_rows + right_rows + left_cost + right_cost;
    }

    // ----------------------------------------------------------------
    // Sort  ≈  N × log2(N) + child_cost
    // ----------------------------------------------------------------
    case T::SORT:
    case T::ORDER_BY: {
        if (node.getChildren().empty()) {
            return kDefaultRows * std::log2(kDefaultRows);
        }
        double n          = estimateRows(*node.getChildren()[0]);
        double child_cost = estimateCost(*node.getChildren()[0]);
        double log2n      = n > 1.0 ? std::log2(n) : 1.0;
        return n * log2n + child_cost;
    }

    // ----------------------------------------------------------------
    // TopK  ≈  N × log2(K) + child_cost
    // ----------------------------------------------------------------
    case T::TOP_K: {
        const auto& topk =
            dynamic_cast<const planner::LogicalTopKNode&>(node);
        double n          = node.getChildren().empty()
                                ? kDefaultRows
                                : estimateRows(*node.getChildren()[0]);
        double child_cost = node.getChildren().empty()
                                ? 0.0
                                : estimateCost(*node.getChildren()[0]);
        double k          = kDefaultK;
        if (const auto* ce =
                dynamic_cast<const ConstantExpression*>(topk.getLimit())) {
            try {
                Tuple empty;
                Schema empty_schema;
                Value v = ce->evaluate(empty, empty_schema);
                if (v.getType() == TypeId::Integer && v.getAsInteger() > 0) {
                    k = static_cast<double>(v.getAsInteger());
                }
            } catch (...) {
                // Ignore evaluation errors; use fallback value.
            }
        }
        double log2k = k > 1.0 ? std::log2(k) : 1.0;
        return n * log2k + child_cost;
    }

    // ----------------------------------------------------------------
    // Pass-through operators — child cost is the dominant term
    // ----------------------------------------------------------------
    case T::PROJECTION:
    case T::HAVING:
    case T::AGGREGATION:
        if (node.getChildren().empty()) {
            return 0.0;
        }
        return estimateCost(*node.getChildren()[0]);

    case T::LIMIT: {
        if (node.getChildren().empty()) {
            return 0.0;
        }
        return estimateCost(*node.getChildren()[0]);
    }

    // ----------------------------------------------------------------
    // VALUES  — constant-time production
    // ----------------------------------------------------------------
    case T::VALUES:
        return 1.0;

    // ----------------------------------------------------------------
    // DML — cost of the source child
    // ----------------------------------------------------------------
    case T::INSERT:
    case T::UPDATE:
    case T::DELETE:
        return node.getChildren().empty()
                   ? 0.0
                   : estimateCost(*node.getChildren()[0]);
    }

    return 0.0; // unreachable
}

} // namespace hamdb::optimizer
