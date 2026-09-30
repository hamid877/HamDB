#include "optimizer/constant_folding_rule.hpp"
#include "executor/constant_expression.hpp"
#include "executor/arithmetic_expression.hpp"
#include "executor/comparison_expression.hpp"
#include "executor/logical_expression.hpp"
#include "planner/logical_index_scan.hpp"
#include "planner/hash_join_plan.hpp"
#include "planner/nested_loop_join_plan.hpp"
#include "planner/aggregation_plan.hpp"
#include "planner/having_plan.hpp"
#include "planner/order_by_plan.hpp"

namespace hamdb::optimizer {

// ---------------------------------------------------------------------------
// Helper: safely evaluate a fully-constant expression subtree.
//
// If the expression throws (e.g. division/modulo by zero, type mismatch)
// we return NULL rather than crashing the optimizer.  This preserves
// HamDB error semantics: a constant expression that is invalid at fold
// time produces NULL; the executor would have thrown the same error at
// runtime, but the optimizer must never abort the query pipeline.
// ---------------------------------------------------------------------------
static Value safeEvaluate(Expression* expr)
{
    try {
        Tuple dummy_tuple;
        Schema dummy_schema;
        return expr->evaluate(dummy_tuple, dummy_schema);
    } catch (...) {
        return Value(); // NULL
    }
}

// ---------------------------------------------------------------------------
// isConstant – returns true iff the expression tree contains only
// ConstantExpression nodes (no column references, parameters, etc.).
// ---------------------------------------------------------------------------
static bool isConstant(const Expression* expr)
{
    if (!expr) return false;
    if (dynamic_cast<const ConstantExpression*>(expr)) return true;
    // A node that is NOT a ConstantExpression itself is constant only if it
    // has children and ALL of them are constant.
    const auto& ch = expr->getChildren();
    if (ch.empty()) return false;
    for (const auto& c : ch) {
        if (!isConstant(c.get())) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// foldExpression – recursively fold a single expression tree.
//
// Invariant: after foldExpression returns, any subtree whose operands were
// all constants has been replaced by a single ConstantExpression holding the
// evaluated result (or NULL on arithmetic error).
//
// Three-valued SQL NULL boolean semantics are respected:
//   false AND x  → false    (x need not be constant)
//   true  AND x  → x
//   x     AND false → false
//   x     AND true  → x
//   true  OR  x  → true     (x need not be constant)
//   false OR  x  → x
//   x     OR  true  → true
//   x     OR  false → x
//   NULL  AND false → false  (handled via right_const path above)
//   NULL  OR  true  → true   (handled via right_const path above)
// ---------------------------------------------------------------------------
std::unique_ptr<Expression> ConstantFoldingRule::foldExpression(
    std::unique_ptr<Expression> expr)
{
    if (!expr) return nullptr;

    // --- Recursively fold children first (bottom-up) --------------------
    auto& children = expr->getMutableChildren();
    for (auto& child : children) {
        child = foldExpression(std::move(child));
    }

    // --- Attempt folding based on expression type -----------------------

    if (auto* arith = dynamic_cast<ArithmeticExpression*>(expr.get())) {
        // Fold if both operands reduced to constants.
        if (isConstant(children[0].get()) && isConstant(children[1].get())) {
            Value result = safeEvaluate(arith);
            return std::make_unique<ConstantExpression>(result);
        }
        return expr;
    }

    if (auto* comp = dynamic_cast<ComparisonExpression*>(expr.get())) {
        // Fold if both operands reduced to constants.
        if (isConstant(children[0].get()) && isConstant(children[1].get())) {
            Value result = safeEvaluate(comp);
            return std::make_unique<ConstantExpression>(result);
        }
        return expr;
    }

    if (auto* logic = dynamic_cast<LogicalExpression*>(expr.get())) {
        if (logic->getLogicType() == LogicalType::Not) {
            // NOT: fold if the sole child is constant.
            if (isConstant(children[0].get())) {
                Value result = safeEvaluate(logic);
                return std::make_unique<ConstantExpression>(result);
            }
            return expr;
        }

        // AND / OR with two children.
        const bool left_is_const  = isConstant(children[0].get());
        const bool right_is_const = isConstant(children[1].get());

        // Both constant → fold entirely.
        if (left_is_const && right_is_const) {
            Value result = safeEvaluate(logic);
            return std::make_unique<ConstantExpression>(result);
        }

        if (logic->getLogicType() == LogicalType::And) {
            // --- Partial short-circuit for AND ---------------------------
            if (left_is_const) {
                Value lv = safeEvaluate(children[0].get());
                if (!lv.isNull()) {
                    if (!lv.getAsBoolean()) {
                        // false AND x  →  false  (regardless of x)
                        return std::make_unique<ConstantExpression>(Value(false));
                    }
                    // true AND x  →  x
                    return std::move(children[1]);
                }
                // lv is NULL:  NULL AND x  →  if x is known-false → false
                //                              otherwise leave as-is
            }
            if (right_is_const) {
                Value rv = safeEvaluate(children[0].get()); // reuse dummy eval
                rv = safeEvaluate(children[1].get());
                if (!rv.isNull()) {
                    if (!rv.getAsBoolean()) {
                        // x AND false  →  false
                        return std::make_unique<ConstantExpression>(Value(false));
                    }
                    // x AND true  →  x
                    return std::move(children[0]);
                }
                // rv is NULL: x AND NULL stays as-is
            }
        } else { // LogicalType::Or
            // --- Partial short-circuit for OR ----------------------------
            if (left_is_const) {
                Value lv = safeEvaluate(children[0].get());
                if (!lv.isNull()) {
                    if (lv.getAsBoolean()) {
                        // true OR x  →  true
                        return std::make_unique<ConstantExpression>(Value(true));
                    }
                    // false OR x  →  x
                    return std::move(children[1]);
                }
                // NULL OR x: if x is known-true → true; else leave as-is
            }
            if (right_is_const) {
                Value rv = safeEvaluate(children[1].get());
                if (!rv.isNull()) {
                    if (rv.getAsBoolean()) {
                        // x OR true  →  true
                        return std::make_unique<ConstantExpression>(Value(true));
                    }
                    // x OR false  →  x
                    return std::move(children[0]);
                }
                // x OR NULL: leave as-is
            }
        }

        return expr;
    }

    // Unknown expression type – return unchanged.
    return expr;
}

// ---------------------------------------------------------------------------
// apply – walk the logical plan tree and fold constant expressions in every
// operator that carries expressions.  The traversal is performed by
// RuleExecutor (bottom-up), so children are already folded before we visit
// the parent.
// ---------------------------------------------------------------------------
std::unique_ptr<planner::LogicalPlanNode> ConstantFoldingRule::apply(
    std::unique_ptr<planner::LogicalPlanNode> plan)
{
    if (!plan) return nullptr;

    switch (plan->getType()) {
        case planner::LogicalPlanType::SEQ_SCAN: {
            auto* node = dynamic_cast<planner::SeqScanPlanNode*>(plan.get());
            if (node && node->getPredicate()) {
                node->setPredicate(foldExpression(node->takePredicate()));
            }
            break;
        }
        case planner::LogicalPlanType::NESTED_LOOP_JOIN: {
            auto* node = dynamic_cast<planner::LogicalNestedLoopJoinNode*>(plan.get());
            if (node && node->getPredicate()) {
                node->setPredicate(foldExpression(node->takePredicate()));
            }
            break;
        }
        case planner::LogicalPlanType::HASH_JOIN: {
            auto* node = dynamic_cast<planner::LogicalHashJoinNode*>(plan.get());
            if (node && node->getLeftKeyExpr()) {
                node->setLeftKeyExpr(foldExpression(node->takeLeftKeyExpr()));
            }
            if (node && node->getRightKeyExpr()) {
                node->setRightKeyExpr(foldExpression(node->takeRightKeyExpr()));
            }
            break;
        }
        case planner::LogicalPlanType::INDEX_SCAN: {
            auto* node = dynamic_cast<planner::LogicalIndexScanNode*>(plan.get());
            if (node && node->getPredicate()) {
                node->predicate_ = foldExpression(node->takePredicate());
            }
            break;
        }
        case planner::LogicalPlanType::FILTER: {
            auto* node = dynamic_cast<planner::FilterPlanNode*>(plan.get());
            if (node && node->getPredicate()) {
                node->setPredicate(foldExpression(node->takePredicate()));
            }
            break;
        }
        case planner::LogicalPlanType::PROJECTION: {
            auto* node = dynamic_cast<planner::ProjectionPlanNode*>(plan.get());
            if (node) {
                for (auto& expr : node->getMutableExpressions()) {
                    expr = foldExpression(std::move(expr));
                }
            }
            break;
        }
        case planner::LogicalPlanType::SORT: {
            auto* node = dynamic_cast<planner::SortPlanNode*>(plan.get());
            if (node) {
                for (auto& pair : node->getMutableOrderBy()) {
                    pair.first = foldExpression(std::move(pair.first));
                }
            }
            break;
        }
        case planner::LogicalPlanType::ORDER_BY: {
            auto* node = dynamic_cast<planner::LogicalOrderByNode*>(plan.get());
            if (node) {
                for (auto& pair : node->getMutableOrderBy()) {
                    pair.first = foldExpression(std::move(pair.first));
                }
            }
            break;
        }
        case planner::LogicalPlanType::LIMIT: {
            auto* node = dynamic_cast<planner::LimitPlanNode*>(plan.get());
            if (node) {
                if (node->getLimit()) {
                    node->setLimit(foldExpression(node->takeLimit()));
                }
                if (node->getOffset()) {
                    node->setOffset(foldExpression(node->takeOffset()));
                }
            }
            break;
        }
        case planner::LogicalPlanType::VALUES: {
            auto* node = dynamic_cast<planner::ValuesPlanNode*>(plan.get());
            if (node) {
                for (auto& row : node->getMutableValues()) {
                    for (auto& expr : row) {
                        expr = foldExpression(std::move(expr));
                    }
                }
            }
            break;
        }
        case planner::LogicalPlanType::UPDATE: {
            auto* node = dynamic_cast<planner::UpdatePlanNode*>(plan.get());
            if (node) {
                for (auto& pair : node->getMutableSetClauses()) {
                    pair.second = foldExpression(std::move(pair.second));
                }
            }
            break;
        }
        case planner::LogicalPlanType::INSERT:
        case planner::LogicalPlanType::DELETE:
            break;
        case planner::LogicalPlanType::AGGREGATION: {
            auto* node = dynamic_cast<planner::LogicalAggregationNode*>(plan.get());
            if (node) {
                for (auto& expr : node->getMutableGroupBys()) {
                    expr = foldExpression(std::move(expr));
                }
                for (auto& expr : node->getMutableAggregates()) {
                    if (expr) {
                        expr = foldExpression(std::move(expr));
                    }
                }
            }
            break;
        }
        case planner::LogicalPlanType::HAVING: {
            auto* node = dynamic_cast<planner::LogicalHavingNode*>(plan.get());
            if (node && node->getPredicate()) {
                node->setPredicate(foldExpression(node->takePredicate()));
            }
            break;
        }
    }
    return plan;
}

} // namespace hamdb::optimizer
