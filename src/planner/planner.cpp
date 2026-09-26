#include "planner/planner.hpp"
#include <stdexcept>

namespace hamdb::planner {

ColumnType Planner::typeIdToColumnType(TypeId type_id) {
    switch (type_id) {
        case TypeId::Integer: return ColumnType::Integer;
        case TypeId::Boolean: return ColumnType::Boolean;
        case TypeId::Varchar: return ColumnType::Varchar;
        default: throw std::runtime_error("Unsupported TypeId to ColumnType conversion");
    }
}

std::unique_ptr<LogicalPlanNode> Planner::plan(std::unique_ptr<binder::BoundStatement> statement) {
    switch (statement->getType()) {
        case binder::BoundStatementType::SELECT:
            return planSelect(static_cast<binder::BoundSelectStatement*>(statement.get())); // NOLINT
        case binder::BoundStatementType::INSERT:
            return planInsert(static_cast<binder::BoundInsertStatement*>(statement.get())); // NOLINT
        case binder::BoundStatementType::UPDATE:
            return planUpdate(static_cast<binder::BoundUpdateStatement*>(statement.get())); // NOLINT
        case binder::BoundStatementType::DELETE:
            return planDelete(static_cast<binder::BoundDeleteStatement*>(statement.get())); // NOLINT
        case binder::BoundStatementType::VALUES:
            return planValues(static_cast<binder::BoundValuesStatement*>(statement.get())); // NOLINT
        default:
            throw std::runtime_error("Unsupported statement type in planner");
    }
}

std::unique_ptr<LogicalPlanNode> Planner::planSelect(binder::BoundSelectStatement* stmt) {
    TableInfo* table_info = nullptr;
    if (catalog_->getTable(stmt->table_name_, table_info) != Status::Ok) {
        throw std::runtime_error("Table not found: " + stmt->table_name_);
    }
    
    // FROM: SeqScan
    auto seq_scan = std::make_unique<SeqScanPlanNode>(table_info->getSchema(), stmt->table_name_, stmt->table_alias_);
    std::unique_ptr<LogicalPlanNode> current_node = std::move(seq_scan);

    // WHERE: Filter
    if (stmt->where_clause_) {
        auto filter = std::make_unique<FilterPlanNode>(current_node->getOutputSchema(), stmt->where_clause_->takeExpr());
        filter->addChild(std::move(current_node));
        current_node = std::move(filter);
    }

    // SELECT: Projection
    if (!stmt->select_list_.empty()) {
        std::vector<Column> columns;
        std::vector<std::unique_ptr<hamdb::Expression>> exprs;
        columns.reserve(stmt->select_list_.size());
        exprs.reserve(stmt->select_list_.size());
        
        for (size_t i = 0; i < stmt->select_list_.size(); ++i) {
            auto& bound_expr = stmt->select_list_[i];
            
            std::string col_name = "expr" + std::to_string(i);
            if (bound_expr->getBoundType() == binder::BoundExpressionType::COLUMN_REF) {
                col_name = static_cast<binder::BoundColumnRef*>(bound_expr.get())->getColumnName(); // NOLINT
            }
            
            ColumnType col_type = typeIdToColumnType(bound_expr->getType());
            columns.emplace_back(col_name, col_type);
            exprs.push_back(bound_expr->takeExpr());
        }
        
        Schema proj_schema(columns);
        auto proj = std::make_unique<ProjectionPlanNode>(std::move(proj_schema), std::move(exprs));
        proj->addChild(std::move(current_node));
        current_node = std::move(proj);
    }

    // ORDER BY: Sort
    if (!stmt->order_by_.empty()) {
        std::vector<std::pair<std::unique_ptr<hamdb::Expression>, bool>> order_by_exprs;
        order_by_exprs.reserve(stmt->order_by_.size());
        for (auto& pair : stmt->order_by_) {
            order_by_exprs.emplace_back(pair.first->takeExpr(), pair.second);
        }
        auto sort = std::make_unique<SortPlanNode>(current_node->getOutputSchema(), std::move(order_by_exprs));
        sort->addChild(std::move(current_node));
        current_node = std::move(sort);
    }

    // LIMIT / OFFSET
    if (stmt->limit_ || stmt->offset_) {
        std::unique_ptr<hamdb::Expression> limit_expr = stmt->limit_ ? stmt->limit_->takeExpr() : nullptr;
        std::unique_ptr<hamdb::Expression> offset_expr = stmt->offset_ ? stmt->offset_->takeExpr() : nullptr;
        
        auto limit_node = std::make_unique<LimitPlanNode>(current_node->getOutputSchema(), std::move(limit_expr), std::move(offset_expr));
        limit_node->addChild(std::move(current_node));
        current_node = std::move(limit_node);
    }

    return current_node;
}

std::unique_ptr<LogicalPlanNode> Planner::planInsert(binder::BoundInsertStatement* stmt) {
    TableInfo* table_info = nullptr;
    if (catalog_->getTable(stmt->table_name_, table_info) != Status::Ok) {
        throw std::runtime_error("Table not found: " + stmt->table_name_);
    }

    std::vector<std::vector<std::unique_ptr<hamdb::Expression>>> values;
    values.reserve(stmt->values_.size());
    for (auto& row : stmt->values_) {
        std::vector<std::unique_ptr<hamdb::Expression>> row_exprs;
        row_exprs.reserve(row.size());
        for (auto& bound_expr : row) {
            row_exprs.push_back(bound_expr->takeExpr());
        }
        values.push_back(std::move(row_exprs));
    }
    
    auto values_node = std::make_unique<ValuesPlanNode>(table_info->getSchema(), std::move(values));
    
    Schema insert_schema({ Column{"affected_rows", ColumnType::Integer} });
    auto insert_node = std::make_unique<InsertPlanNode>(insert_schema, stmt->table_name_);
    insert_node->addChild(std::move(values_node));
    return insert_node;
}

std::unique_ptr<LogicalPlanNode> Planner::planUpdate(binder::BoundUpdateStatement* stmt) {
    TableInfo* table_info = nullptr;
    if (catalog_->getTable(stmt->table_name_, table_info) != Status::Ok) {
        throw std::runtime_error("Table not found: " + stmt->table_name_);
    }
    
    auto seq_scan = std::make_unique<SeqScanPlanNode>(table_info->getSchema(), stmt->table_name_, "");
    std::unique_ptr<LogicalPlanNode> current_node = std::move(seq_scan);
    
    if (stmt->where_clause_) {
        auto filter = std::make_unique<FilterPlanNode>(current_node->getOutputSchema(), stmt->where_clause_->takeExpr());
        filter->addChild(std::move(current_node));
        current_node = std::move(filter);
    }
    
    std::vector<std::pair<std::string, std::unique_ptr<hamdb::Expression>>> set_clauses;
    set_clauses.reserve(stmt->set_clauses_.size());
    for (auto& pair : stmt->set_clauses_) {
        set_clauses.emplace_back(pair.first, pair.second->takeExpr());
    }
    
    Schema update_schema({ Column{"affected_rows", ColumnType::Integer} });
    auto update_node = std::make_unique<UpdatePlanNode>(update_schema, stmt->table_name_, std::move(set_clauses));
    update_node->addChild(std::move(current_node));
    return update_node;
}

std::unique_ptr<LogicalPlanNode> Planner::planDelete(binder::BoundDeleteStatement* stmt) {
    TableInfo* table_info = nullptr;
    if (catalog_->getTable(stmt->table_name_, table_info) != Status::Ok) {
        throw std::runtime_error("Table not found: " + stmt->table_name_);
    }
    
    auto seq_scan = std::make_unique<SeqScanPlanNode>(table_info->getSchema(), stmt->table_name_, "");
    std::unique_ptr<LogicalPlanNode> current_node = std::move(seq_scan);
    
    if (stmt->where_clause_) {
        auto filter = std::make_unique<FilterPlanNode>(current_node->getOutputSchema(), stmt->where_clause_->takeExpr());
        filter->addChild(std::move(current_node));
        current_node = std::move(filter);
    }
    
    Schema delete_schema({ Column{"affected_rows", ColumnType::Integer} });
    auto delete_node = std::make_unique<DeletePlanNode>(delete_schema, stmt->table_name_);
    delete_node->addChild(std::move(current_node));
    return delete_node;
}

std::unique_ptr<LogicalPlanNode> Planner::planValues(binder::BoundValuesStatement* stmt) {
    std::vector<Column> columns;
    if (!stmt->values_.empty() && !stmt->values_[0].empty()) {
        columns.reserve(stmt->values_[0].size());
        for (size_t i = 0; i < stmt->values_[0].size(); ++i) {
            ColumnType col_type = typeIdToColumnType(stmt->values_[0][i]->getType());
            columns.emplace_back("val" + std::to_string(i), col_type);
        }
    }
    Schema values_schema(columns);
    
    std::vector<std::vector<std::unique_ptr<hamdb::Expression>>> values;
    values.reserve(stmt->values_.size());
    for (auto& row : stmt->values_) {
        std::vector<std::unique_ptr<hamdb::Expression>> row_exprs;
        row_exprs.reserve(row.size());
        for (auto& bound_expr : row) {
            row_exprs.push_back(bound_expr->takeExpr());
        }
        values.push_back(std::move(row_exprs));
    }
    
    return std::make_unique<ValuesPlanNode>(values_schema, std::move(values));
}

} // namespace hamdb::planner
