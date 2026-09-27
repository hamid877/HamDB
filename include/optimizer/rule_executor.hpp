#pragma once
#include "optimizer/rule.hpp"
#include <vector>

namespace hamdb::optimizer {

class RuleExecutor {
public:
    void addRule(std::unique_ptr<Rule> rule);
    std::unique_ptr<planner::LogicalPlanNode> optimize(std::unique_ptr<planner::LogicalPlanNode> plan);

    const std::vector<std::string>& getAppliedRules() const { return applied_rules_; }
    void clearAppliedRules() { applied_rules_.clear(); }
    void addAppliedRule(const std::string& rule) { applied_rules_.push_back(rule); }

private:
    std::vector<std::unique_ptr<Rule>> rules_;
    std::vector<std::string> applied_rules_;
};

} // namespace hamdb::optimizer
