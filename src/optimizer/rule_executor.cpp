#include "optimizer/rule_executor.hpp"
#include <algorithm>

namespace hamdb::optimizer {

void RuleExecutor::addRule(std::unique_ptr<Rule> rule) {
    rules_.push_back(std::move(rule));
}

std::unique_ptr<planner::LogicalPlanNode> RuleExecutor::optimize(std::unique_ptr<planner::LogicalPlanNode> plan) {
    if (!plan) return nullptr;

    // Recursively optimize children first (bottom-up rewrite)
    for (auto& child : plan->getChildren()) {
        child = optimize(std::move(child));
    }

    // Apply rules to the current node
    for (auto& rule : rules_) {
        plan = rule->apply(std::move(plan));
        if (std::find(applied_rules_.begin(), applied_rules_.end(), rule->name()) == applied_rules_.end()) {
            applied_rules_.push_back(rule->name());
        }
    }

    return plan;
}

} // namespace hamdb::optimizer
