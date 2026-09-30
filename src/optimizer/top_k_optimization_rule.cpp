#include "optimizer/top_k_optimization_rule.hpp"
#include "planner/top_k_plan.hpp"
#include "planner/logical_plan.hpp"
#include "planner/order_by_plan.hpp"

namespace hamdb::optimizer {

std::unique_ptr<planner::LogicalPlanNode> TopKOptimizationRule::apply(std::unique_ptr<planner::LogicalPlanNode> plan) {
    if (!plan) return nullptr;

    // Process children first (bottom-up approach ensures nested limits/orders are handled)
    for (auto& child : plan->getChildren()) {
        child = apply(std::move(child));
    }

    if (plan->getType() == planner::LogicalPlanType::LIMIT) {
        auto* limit_node = dynamic_cast<planner::LimitPlanNode*>(plan.get());
        
        if (limit_node->getChildren().size() == 1 && limit_node->getChildren()[0]->getType() == planner::LogicalPlanType::ORDER_BY) {
            auto order_by_node = std::unique_ptr<planner::LogicalOrderByNode>(
                dynamic_cast<planner::LogicalOrderByNode*>(limit_node->getChildren()[0].release()));
            
            auto top_k_node = std::make_unique<planner::LogicalTopKNode>(
                limit_node->getOutputSchema(),
                std::move(order_by_node->getMutableOrderBy()),
                limit_node->takeLimit(),
                limit_node->takeOffset()
            );

            // Transfer the children from the ORDER_BY node to the TOP_K node
            for (auto& order_child : order_by_node->getChildren()) {
                top_k_node->addChild(std::move(order_child));
            }
            
            return top_k_node;
        }
    }

    return plan;
}

} // namespace hamdb::optimizer
