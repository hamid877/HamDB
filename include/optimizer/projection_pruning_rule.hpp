#pragma once

#include "optimizer/rule.hpp"
#include <unordered_set>

namespace hamdb::optimizer {

class ProjectionPruningRule : public Rule {
public:
    std::unique_ptr<planner::LogicalPlanNode> apply(std::unique_ptr<planner::LogicalPlanNode> plan) override;
    std::string name() const override { return "ProjectionPruningRule"; }

private:
    void collectColumns(const Expression* expr, std::unordered_set<uint32_t>& required_cols);
    std::unique_ptr<planner::LogicalPlanNode> rewriteTopDown(std::unique_ptr<planner::LogicalPlanNode> node, std::unordered_set<uint32_t> required_cols);
};

} // namespace hamdb::optimizer
