#include "optimizer/sort_limit_rule.hpp"
#include "planner/logical_plan.hpp"
#include "planner/order_by_plan.hpp"
#include "planner/logical_index_scan.hpp"
#include "executor/column_value_expression.hpp"

namespace hamdb::optimizer {

std::unique_ptr<planner::LogicalPlanNode> SortLimitRule::apply(std::unique_ptr<planner::LogicalPlanNode> plan) {
    if (!plan) return nullptr;

    for (auto& child : plan->getChildren()) {
        child = apply(std::move(child));
    }

    if (plan->getType() == planner::LogicalPlanType::SORT) {
        auto* sort_node = static_cast<planner::SortPlanNode*>(plan.get());
        bool can_eliminate = false;

        if (sort_node->getOrderBy().size() == 1) {
            auto& order_pair = sort_node->getOrderBy()[0];
            auto* sort_expr = dynamic_cast<ColumnValueExpression*>(order_pair.first.get());
            bool is_asc = order_pair.second;

            if (sort_expr && is_asc) {
                uint32_t current_col_idx = sort_expr->getColIdx();
                planner::LogicalPlanNode* current_node = sort_node->getChildren()[0].get();
                bool match_found = false;

                while (current_node) {
                    if (current_node->getType() == planner::LogicalPlanType::PROJECTION) {
                        auto* proj = static_cast<planner::ProjectionPlanNode*>(current_node);
                        if (current_col_idx < proj->getExpressions().size()) {
                            auto* proj_expr = dynamic_cast<ColumnValueExpression*>(proj->getExpressions()[current_col_idx].get());
                            if (!proj_expr) break;
                            current_col_idx = proj_expr->getColIdx();
                            current_node = proj->getChildren()[0].get();
                        } else {
                            break;
                        }
                    } else if (current_node->getType() == planner::LogicalPlanType::FILTER) {
                        current_node = current_node->getChildren()[0].get();
                    } else if (current_node->getType() == planner::LogicalPlanType::INDEX_SCAN) {
                        if (current_col_idx == 0) {
                            match_found = true;
                        }
                        break;
                    } else {
                        break;
                    }
                }

                if (match_found) {
                    can_eliminate = true;
                }
            }
        }

        if (can_eliminate) {
            return std::move(sort_node->getChildren()[0]);
        }
    } else if (plan->getType() == planner::LogicalPlanType::ORDER_BY) {
        auto* ob_node = static_cast<planner::LogicalOrderByNode*>(plan.get());
        bool can_eliminate = false;

        if (ob_node->getOrderBy().size() == 1) {
            auto& order_pair = ob_node->getOrderBy()[0];
            auto* sort_expr = dynamic_cast<ColumnValueExpression*>(order_pair.first.get());
            bool is_asc = order_pair.second;

            if (sort_expr && is_asc) {
                uint32_t current_col_idx = sort_expr->getColIdx();
                planner::LogicalPlanNode* current_node = ob_node->getChildren()[0].get();
                bool match_found = false;

                while (current_node) {
                    if (current_node->getType() == planner::LogicalPlanType::PROJECTION) {
                        auto* proj = static_cast<planner::ProjectionPlanNode*>(current_node);
                        if (current_col_idx < proj->getExpressions().size()) {
                            auto* proj_expr = dynamic_cast<ColumnValueExpression*>(proj->getExpressions()[current_col_idx].get());
                            if (!proj_expr) break;
                            current_col_idx = proj_expr->getColIdx();
                            current_node = proj->getChildren()[0].get();
                        } else {
                            break;
                        }
                    } else if (current_node->getType() == planner::LogicalPlanType::FILTER) {
                        current_node = current_node->getChildren()[0].get();
                    } else if (current_node->getType() == planner::LogicalPlanType::INDEX_SCAN) {
                        if (current_col_idx == 0) {
                            match_found = true;
                        }
                        break;
                    } else {
                        break;
                    }
                }

                if (match_found) {
                    can_eliminate = true;
                }
            }
        }

        if (can_eliminate) {
            return std::move(ob_node->getChildren()[0]);
        }
    } else if (plan->getType() == planner::LogicalPlanType::LIMIT) {
        auto* limit_node = static_cast<planner::LimitPlanNode*>(plan.get());
        planner::LogicalPlanNode* current_node = limit_node->getChildren()[0].get();
        planner::LogicalPlanNode* target_scan = nullptr;

        while (current_node) {
            if (current_node->getType() == planner::LogicalPlanType::PROJECTION ||
                current_node->getType() == planner::LogicalPlanType::FILTER) {
                current_node = current_node->getChildren()[0].get();
            } else if (current_node->getType() == planner::LogicalPlanType::SEQ_SCAN ||
                       current_node->getType() == planner::LogicalPlanType::INDEX_SCAN) {
                target_scan = current_node;
                break;
            } else {
                break;
            }
        }

        if (target_scan) {
            if (target_scan->getType() == planner::LogicalPlanType::SEQ_SCAN) {
                auto* seq_scan = static_cast<planner::SeqScanPlanNode*>(target_scan);
                seq_scan->setLimit(limit_node->takeLimit());
                seq_scan->setOffset(limit_node->takeOffset());
            } else {
                auto* index_scan = static_cast<planner::LogicalIndexScanNode*>(target_scan);
                index_scan->setLimit(limit_node->takeLimit());
                index_scan->setOffset(limit_node->takeOffset());
            }
            return std::move(limit_node->getChildren()[0]);
        }
    }

    return plan;
}

} // namespace hamdb::optimizer
