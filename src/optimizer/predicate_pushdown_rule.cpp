#include "optimizer/predicate_pushdown_rule.hpp"
#include "executor/logical_expression.hpp"

namespace hamdb::optimizer {

std::unique_ptr<planner::LogicalPlanNode> PredicatePushdownRule::apply(std::unique_ptr<planner::LogicalPlanNode> plan) {
    if (!plan) return nullptr;

    if (plan->getType() == planner::LogicalPlanType::FILTER) {
        auto* filter_node = dynamic_cast<planner::FilterPlanNode*>(plan.get());
        if (filter_node != nullptr && filter_node->getChildren().size() == 1) {
            auto& child = filter_node->getChildren()[0];
            if (child->getType() == planner::LogicalPlanType::SEQ_SCAN) {
                auto* seq_scan = dynamic_cast<planner::SeqScanPlanNode*>(child.get());
                if (seq_scan != nullptr) {
                    auto filter_pred = filter_node->takePredicate();
                    if (seq_scan->getPredicate() != nullptr) {
                        auto old_pred = seq_scan->takePredicate();
                        auto and_expr = std::make_unique<LogicalExpression>(LogicalType::And, std::move(old_pred), std::move(filter_pred));
                        seq_scan->setPredicate(std::move(and_expr));
                    } else {
                        seq_scan->setPredicate(std::move(filter_pred));
                    }
                    
                    return std::move(child);
                }
            }
        }
    }
    
    return plan;
}

} // namespace hamdb::optimizer
