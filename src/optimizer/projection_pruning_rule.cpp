#include "optimizer/projection_pruning_rule.hpp"
#include "executor/column_value_expression.hpp"
#include "planner/logical_index_scan.hpp"
#include "planner/order_by_plan.hpp"
#include <vector>
#include <algorithm>

namespace hamdb::optimizer {

std::unique_ptr<planner::LogicalPlanNode> ProjectionPruningRule::apply(std::unique_ptr<planner::LogicalPlanNode> plan) {
    if (!plan) return nullptr;

    auto type = plan->getType();
    if (type == planner::LogicalPlanType::INSERT ||
        type == planner::LogicalPlanType::UPDATE ||
        type == planner::LogicalPlanType::DELETE ||
        type == planner::LogicalPlanType::VALUES) {
        return plan;
    }

    std::unordered_set<uint32_t> required_cols;
    for (uint32_t i = 0; i < plan->getOutputSchema().getColumnCount(); ++i) {
        required_cols.insert(i);
    }
    
    return rewriteTopDown(std::move(plan), required_cols);
}

void ProjectionPruningRule::collectColumns(const Expression* expr, std::unordered_set<uint32_t>& required_cols) {
    if (expr == nullptr) return;
    if (const auto* col_expr = dynamic_cast<const ColumnValueExpression*>(expr)) {
        required_cols.insert(col_expr->getColIdx());
    }
    for (const auto& child : expr->getChildren()) {
        collectColumns(child.get(), required_cols);
    }
}

std::unique_ptr<planner::LogicalPlanNode> ProjectionPruningRule::rewriteTopDown(
    std::unique_ptr<planner::LogicalPlanNode> node, std::unordered_set<uint32_t> required_cols) {
    
    if (!node) return nullptr;
    
    auto type = node->getType();
    
    if (type == planner::LogicalPlanType::PROJECTION) {
        auto* proj = dynamic_cast<planner::ProjectionPlanNode*>(node.get());
        
        bool is_wildcard = false;
        if (!proj->getChildren().empty()) {
            const auto& child_schema = proj->getChildren()[0]->getOutputSchema();
            if (proj->getExpressions().size() == child_schema.getColumnCount()) {
                is_wildcard = true;
                for (size_t i = 0; i < proj->getExpressions().size(); ++i) {
                    const auto* col_expr = dynamic_cast<const ColumnValueExpression*>(proj->getExpressions()[i].get());
                    if (col_expr == nullptr || col_expr->getColIdx() != i) {
                        is_wildcard = false;
                        break;
                    }
                }
            }
        }

        std::unordered_set<uint32_t> child_required;
        if (is_wildcard) {
            for (uint32_t i = 0; i < proj->getChildren()[0]->getOutputSchema().getColumnCount(); ++i) {
                child_required.insert(i);
            }
        } else {
            for (const auto& expr : proj->getExpressions()) {
                collectColumns(expr.get(), child_required);
            }
        }
        
        for (auto& child : node->getChildren()) {
            child = rewriteTopDown(std::move(child), child_required);
        }
        return node;
    } 
    
    if (type == planner::LogicalPlanType::FILTER) {
        auto* filter = dynamic_cast<planner::FilterPlanNode*>(node.get());
        std::unordered_set<uint32_t> child_required = required_cols;
        collectColumns(filter->getPredicate(), child_required);
        
        for (auto& child : node->getChildren()) {
            child = rewriteTopDown(std::move(child), child_required);
        }
        return node;
    }
    
    if (type == planner::LogicalPlanType::SORT) {
        auto* sort = dynamic_cast<planner::SortPlanNode*>(node.get());
        std::unordered_set<uint32_t> child_required = required_cols;
        for (const auto& pair : sort->getOrderBy()) {
            collectColumns(pair.first.get(), child_required);
        }
        
        for (auto& child : node->getChildren()) {
            child = rewriteTopDown(std::move(child), child_required);
        }
        return node;
    }

    if (type == planner::LogicalPlanType::ORDER_BY) {
        auto* ob = dynamic_cast<planner::LogicalOrderByNode*>(node.get());
        std::unordered_set<uint32_t> child_required = required_cols;
        for (const auto& pair : ob->getOrderBy()) {
            collectColumns(pair.first.get(), child_required);
        }
        
        for (auto& child : node->getChildren()) {
            child = rewriteTopDown(std::move(child), child_required);
        }
        return node;
    }
    
    if (type == planner::LogicalPlanType::LIMIT) {
        auto* limit = dynamic_cast<planner::LimitPlanNode*>(node.get());
        std::unordered_set<uint32_t> child_required = required_cols;
        collectColumns(limit->getLimit(), child_required);
        collectColumns(limit->getOffset(), child_required);
        
        for (auto& child : node->getChildren()) {
            child = rewriteTopDown(std::move(child), child_required);
        }
        return node;
    }
    
    if (type == planner::LogicalPlanType::SEQ_SCAN) {
        auto* scan = dynamic_cast<planner::SeqScanPlanNode*>(node.get());
        
        collectColumns(scan->getPredicate(), required_cols);
        
        const auto& old_schema = scan->getOutputSchema();
        std::vector<Column> new_columns;
        for (uint32_t i = 0; i < old_schema.getColumnCount(); ++i) {
            if (required_cols.find(i) != required_cols.end()) {
                new_columns.push_back(old_schema.getColumn(i));
            }
        }
        
        if (new_columns.empty() && old_schema.getColumnCount() > 0) {
            new_columns.push_back(old_schema.getColumn(0));
        }
        
        if (new_columns.size() == old_schema.getColumnCount()) {
            return node;
        }
        
        Schema new_schema(new_columns);
        auto new_scan = std::make_unique<planner::SeqScanPlanNode>(
            std::move(new_schema), scan->getTableName(), scan->getTableAlias(), scan->takePredicate());
            
        return new_scan;
    }
    
    if (type == planner::LogicalPlanType::INDEX_SCAN) {
        auto* scan = dynamic_cast<planner::LogicalIndexScanNode*>(node.get());
        
        collectColumns(scan->getPredicate(), required_cols);
        
        const auto& old_schema = scan->getOutputSchema();
        std::vector<Column> new_columns;
        for (uint32_t i = 0; i < old_schema.getColumnCount(); ++i) {
            if (required_cols.find(i) != required_cols.end()) {
                new_columns.push_back(old_schema.getColumn(i));
            }
        }
        
        if (new_columns.empty() && old_schema.getColumnCount() > 0) {
            new_columns.push_back(old_schema.getColumn(0));
        }
        
        if (new_columns.size() == old_schema.getColumnCount()) {
            return node;
        }
        
        Schema new_schema(new_columns);
        auto new_scan = std::make_unique<planner::LogicalIndexScanNode>(
            std::move(new_schema), scan->getTableName(), scan->getTableAlias(), scan->takePredicate());
            
        return new_scan;
    }
    
    for (auto& child : node->getChildren()) {
        child = rewriteTopDown(std::move(child), required_cols);
    }
    return node;
}

} // namespace hamdb::optimizer
