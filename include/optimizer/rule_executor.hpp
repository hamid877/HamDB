#pragma once
#include "optimizer/rule.hpp"
#include <vector>

namespace hamdb::optimizer {

class RuleExecutor {
public:
    void addRule(std::unique_ptr<Rule> rule);
    std::unique_ptr<planner::LogicalPlanNode> optimize(std::unique_ptr<planner::LogicalPlanNode> plan);

private:
    std::vector<std::unique_ptr<Rule>> rules_;
};

} // namespace hamdb::optimizer
