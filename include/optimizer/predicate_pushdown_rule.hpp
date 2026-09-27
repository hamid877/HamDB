#pragma once
#include "optimizer/rule.hpp"

namespace hamdb::optimizer {

class PredicatePushdownRule : public Rule {
public:
    std::unique_ptr<planner::LogicalPlanNode> apply(std::unique_ptr<planner::LogicalPlanNode> plan) override;
    std::string name() const override { return "PredicatePushdownRule"; }
};

} // namespace hamdb::optimizer
