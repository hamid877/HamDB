#include "optimizer/cost_based_join_rule.hpp"
#include "executor/column_value_expression.hpp"
#include "executor/comparison_expression.hpp"
#include "executor/logical_expression.hpp"
#include "planner/nested_loop_join_plan.hpp"
#include "planner/hash_join_plan.hpp"
#include "planner/logical_plan.hpp"
#include <vector>
#include <utility>

namespace hamdb::optimizer {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

CostBasedJoinRule::CostBasedJoinRule(CostModel cost_model)
    : cost_model_(std::move(cost_model))
{
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

namespace {

/// Clone a ColumnValueExpression with an optional column-index shift.
std::unique_ptr<hamdb::Expression>
cloneShiftedColumn(const hamdb::ColumnValueExpression* col, int shift)
{
    return std::make_unique<hamdb::ColumnValueExpression>(
        static_cast<uint32_t>(static_cast<int>(col->getColIdx()) + shift));
}

/// Collect all ColumnValueExpression column indices in expr.
void collectColumns(const hamdb::Expression* expr,
                    std::vector<uint32_t>& out)
{
    if (!expr) {
        return;
    }
    if (const auto* c =
            dynamic_cast<const hamdb::ColumnValueExpression*>(expr)) {
        out.push_back(c->getColIdx());
    }
    for (const auto& child : expr->getChildren()) {
        collectColumns(child.get(), out);
    }
}

bool allBelow(const std::vector<uint32_t>& cols, uint32_t threshold)
{
    for (uint32_t c : cols) {
        if (c >= threshold) {
            return false;
        }
    }
    return true;
}

bool allAboveOrEqual(const std::vector<uint32_t>& cols, uint32_t threshold)
{
    for (uint32_t c : cols) {
        if (c < threshold) {
            return false;
        }
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// extractEquiKeys
//
// Scans the top-level predicate (first equality conjunct only) looking for an
// equi-join condition of the form  left_col = right_col  where left_col
// references only left child columns (<left_col_count) and right_col only
// right child columns (>=left_col_count).
//
// On success: fills left_key and right_key, returns true.
// right_key column index is re-based to 0 (right-child local).
// ---------------------------------------------------------------------------

bool extractEquiKeys(
    const hamdb::Expression* pred,
    uint32_t left_col_count,
    std::unique_ptr<hamdb::Expression>& left_key,
    std::unique_ptr<hamdb::Expression>& right_key)
{
    if (pred == nullptr) {
        return false;
    }

    // Recurse into AND conjuncts — we only need the *first* equi-join key.
    if (const auto* logic =
            dynamic_cast<const hamdb::LogicalExpression*>(pred)) {
        if (logic->getLogicType() == hamdb::LogicalType::And) {
            const auto& lc = logic->getChildren();
            if (extractEquiKeys(lc[0].get(), left_col_count,
                                left_key, right_key)) {
                return true;
            }
            if (extractEquiKeys(lc[1].get(), left_col_count,
                                left_key, right_key)) {
                return true;
            }
        }
        return false;
    }

    const auto* comp = dynamic_cast<const hamdb::ComparisonExpression*>(pred);
    if ((comp == nullptr) ||
        comp->getComparisonType() != hamdb::ComparisonType::Equal) {
        return false;
    }

    const auto& lc = comp->getChildren();
    if (lc.size() < 2) {
        return false;
    }

    const hamdb::Expression* lhs = lc[0].get();
    const hamdb::Expression* rhs = lc[1].get();

    std::vector<uint32_t> lhs_cols;
    std::vector<uint32_t> rhs_cols;
    collectColumns(lhs, lhs_cols);
    collectColumns(rhs, rhs_cols);

    if (lhs_cols.empty() || rhs_cols.empty()) {
        return false;
    }

    // Case: lhs → left child, rhs → right child
    if (allBelow(lhs_cols, left_col_count) &&
        allAboveOrEqual(rhs_cols, left_col_count)) {
        const auto* l_col =
            dynamic_cast<const hamdb::ColumnValueExpression*>(lhs);
        const auto* r_col =
            dynamic_cast<const hamdb::ColumnValueExpression*>(rhs);
        if ((l_col != nullptr) && (r_col != nullptr)) {
            left_key  = cloneShiftedColumn(l_col, 0);
            right_key = cloneShiftedColumn(
                r_col, -static_cast<int>(left_col_count));
            return true;
        }
    }

    // Case: rhs → left child, lhs → right child
    if (allAboveOrEqual(lhs_cols, left_col_count) &&
        allBelow(rhs_cols, left_col_count)) {
        const auto* l_col =
            dynamic_cast<const hamdb::ColumnValueExpression*>(rhs);
        const auto* r_col =
            dynamic_cast<const hamdb::ColumnValueExpression*>(lhs);
        if ((l_col != nullptr) && (r_col != nullptr)) {
            left_key  = cloneShiftedColumn(l_col, 0);
            right_key = cloneShiftedColumn(
                r_col, -static_cast<int>(left_col_count));
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// apply
// ---------------------------------------------------------------------------

std::unique_ptr<planner::LogicalPlanNode>
CostBasedJoinRule::apply(std::unique_ptr<planner::LogicalPlanNode> plan)
{
    if (!plan) {
        return nullptr;
    }

    // Apply recursively bottom-up.
    auto& children = plan->getChildren();
    for (auto& child : children) {
        child = apply(std::move(child));
    }

    // Only handle NestedLoopJoin nodes here.  HashJoin nodes produced by the
    // earlier JoinSelectionRule are already equi-joins; we do *not* re-inspect
    // them to avoid double-processing and keep the rule set composable.
    if (plan->getType() != planner::LogicalPlanType::NESTED_LOOP_JOIN) {
        return plan;
    }

    auto* nlj = dynamic_cast<planner::LogicalNestedLoopJoinNode*>(plan.get());
    if ((nlj == nullptr) || nlj->getChildren().size() < 2) {
        return plan;
    }

    uint32_t left_col_count =
        nlj->getChildren()[0]->getOutputSchema().getColumnCount();

    // Try to extract an equi-join key pair from the predicate.
    std::unique_ptr<hamdb::Expression> left_key;
    std::unique_ptr<hamdb::Expression> right_key;

    const auto* pred_ptr = nlj->getPredicate();

    if (!extractEquiKeys(pred_ptr, left_col_count, left_key, right_key)) {
        // Non-equi join — NLJ is the only valid choice.
        return plan;
    }

    // --- Cost comparison ---
    // NLJ cost: already computed from the existing plan tree.
    double nlj_cost = cost_model_.estimateCost(*nlj);

    // Build a hypothetical HashJoin node with the same children (we only
    // need it for cost estimation, not execution).
    // Borrow the schema from NLJ (same output).
    auto hash_candidate = std::make_unique<planner::LogicalHashJoinNode>(
        nlj->getOutputSchema(),
        std::make_unique<hamdb::ColumnValueExpression>(
            dynamic_cast<const hamdb::ColumnValueExpression&>(*left_key)
                .getColIdx()),
        std::make_unique<hamdb::ColumnValueExpression>(
            dynamic_cast<const hamdb::ColumnValueExpression&>(*right_key)
                .getColIdx()));

    // Temporarily attach child cost proxies (we only need estimateRows, not
    // deep copies of children, so we create lightweight SeqScan proxies with
    // the correct schema so estimateRows can find the column count).
    //
    // For estimation purposes we read rows directly from the child nodes that
    // are already in the plan tree.
    double left_rows  = cost_model_.estimateRows(*nlj->getChildren()[0]);
    double right_rows = cost_model_.estimateRows(*nlj->getChildren()[1]);
    double left_cost  = cost_model_.estimateCost(*nlj->getChildren()[0]);
    double right_cost = cost_model_.estimateCost(*nlj->getChildren()[1]);

    double hash_cost = left_rows + right_rows + left_cost + right_cost;

    if (hash_cost >= nlj_cost) {
        // NLJ is cheaper (or a tie): keep it unchanged.
        return plan;
    }

    // HashJoin is cheaper: promote.
    auto hash_join = std::make_unique<planner::LogicalHashJoinNode>(
        nlj->getOutputSchema(),
        std::move(left_key),
        std::move(right_key));

    hash_join->addChild(std::move(nlj->getChildren()[0]));
    hash_join->addChild(std::move(nlj->getChildren()[1]));

    return hash_join;
}

} // namespace hamdb::optimizer
