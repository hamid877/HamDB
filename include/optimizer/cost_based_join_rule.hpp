#pragma once

#include "optimizer/rule.hpp"
#include "optimizer/cost_model.hpp"
#include <memory>

namespace hamdb::optimizer {

/**
 * @brief Cost-based join algorithm selection rule.
 *
 * For every LogicalNestedLoopJoinNode that carries a supported equi-join
 * predicate, this rule generates two candidate plans:
 *   1. NestedLoopJoin  (the existing plan, unchanged)
 *   2. HashJoin        (same as produced by JoinSelectionRule)
 *
 * It then uses CostModel to estimate both costs and keeps the cheaper one.
 * Non-equi joins always stay as NestedLoopJoin because HashJoin does not
 * support them.
 *
 * This rule is registered *after* JoinSelectionRule in HamDBOptimizer so that
 * it can also reconsider joins that were already promoted to HashJoin by the
 * earlier rule.  When it encounters a LogicalHashJoinNode it compares it
 * against the equivalent NLJ cost and downgrades if NLJ would be cheaper
 * (e.g. very small tables).
 */
class CostBasedJoinRule : public Rule {
public:
    explicit CostBasedJoinRule(CostModel cost_model = CostModel{});

    std::unique_ptr<planner::LogicalPlanNode>
    apply(std::unique_ptr<planner::LogicalPlanNode> plan) override;

    std::string name() const override { return "CostBasedJoinRule"; }

private:
    CostModel cost_model_;
};

} // namespace hamdb::optimizer
