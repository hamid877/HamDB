#pragma once

#include "optimizer/rule.hpp"

namespace hamdb::optimizer {

class SortLimitRule : public Rule {
public:
    std::unique_ptr<planner::LogicalPlanNode> apply(std::unique_ptr<planner::LogicalPlanNode> plan) override;
    std::string name() const override { return "SortLimitRule"; }
};

} // namespace hamdb::optimizer
