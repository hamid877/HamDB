#pragma once
#include "planner/logical_plan.hpp"
#include <memory>

namespace hamdb::optimizer {

class Rule {
public:
    virtual ~Rule() = default;
    virtual std::unique_ptr<planner::LogicalPlanNode> apply(std::unique_ptr<planner::LogicalPlanNode> plan) = 0;
};

} // namespace hamdb::optimizer
