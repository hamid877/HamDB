#pragma once

#include "optimizer/rule.hpp"
#include <memory>

namespace hamdb::optimizer {

class TopKOptimizationRule : public Rule {
public:
    std::unique_ptr<planner::LogicalPlanNode> apply(std::unique_ptr<planner::LogicalPlanNode> plan) override;
    std::string name() const override { return "TopKOptimizationRule"; }
};

} // namespace hamdb::optimizer
