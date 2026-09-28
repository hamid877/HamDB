#include "binder/binder.hpp"
#include <algorithm>
#include <cctype>
#include <functional>

namespace hamdb::binder {

TypeId Binder::columnTypeToTypeId(ColumnType type) {
    switch (type) {
        case ColumnType::Integer: return TypeId::Integer;
        case ColumnType::Boolean: return TypeId::Boolean;
        case ColumnType::Varchar: return TypeId::Varchar;
        default: throw BinderError("Unsupported column type");
    }
}

std::unique_ptr<BoundStatement> Binder::bind(const ast::Statement& stmt) {
    if (auto s = dynamic_cast<const ast::SelectStatement*>(&stmt)) return bindSelect(*s);
    if (auto s = dynamic_cast<const ast::InsertStatement*>(&stmt)) return bindInsert(*s);
    if (auto s = dynamic_cast<const ast::UpdateStatement*>(&stmt)) return bindUpdate(*s);
    if (auto s = dynamic_cast<const ast::DeleteStatement*>(&stmt)) return bindDelete(*s);
    if (auto s = dynamic_cast<const ast::ValuesStatement*>(&stmt)) return bindValues(*s);
    throw BinderError("Unknown statement type");
}

std::unique_ptr<BoundTableReference> Binder::bindTableReference(const ast::TableReference& ref, std::vector<Column>& combined_columns) {
    if (auto base = dynamic_cast<const ast::BaseTableReference*>(&ref)) {
        auto bound_base = std::make_unique<BoundBaseTableReference>();
        TableInfo* table_info = nullptr;
        if (catalog_->getTable(base->table_name_, table_info) != Status::Ok) {
            throw BinderError("Unknown table: " + base->table_name_);
        }
        bound_base->table_name_ = base->table_name_;
        bound_base->table_alias_ = base->table_alias_;
        bound_base->schema_ = &table_info->getSchema();
        
        TableContext tc;
        tc.table_name = base->table_name_;
        tc.table_alias = base->table_alias_;
        tc.schema = bound_base->schema_;
        tc.column_offset = combined_columns.size();
        current_context_.tables.push_back(tc);
        
        for (uint32_t i = 0; i < tc.schema->getColumnCount(); ++i) {
            combined_columns.push_back(tc.schema->getColumn(i));
        }
        return bound_base;
    } else if (auto join = dynamic_cast<const ast::JoinTableReference*>(&ref)) {
        auto bound_join = std::make_unique<BoundJoinTable>();
        bound_join->left_ = bindTableReference(*join->left_, combined_columns);
        bound_join->right_ = bindTableReference(*join->right_, combined_columns);
        
        current_context_.combined_schema = std::make_unique<Schema>(combined_columns);
        if (join->condition_) {
            bound_join->condition_ = bindExpression(*join->condition_);
            if (bound_join->condition_->getType() != TypeId::Boolean) {
                throw BinderError("JOIN condition must be of boolean type");
            }
        }
        return bound_join;
    }
    throw BinderError("Unknown TableReference type");
}

std::unique_ptr<BoundSelectStatement> Binder::bindSelect(const ast::SelectStatement& stmt) {
    auto bound = std::make_unique<BoundSelectStatement>();
    current_context_.tables.clear();
    
    if (stmt.table) {
        std::vector<Column> combined_columns;
        bound->table_ = bindTableReference(*stmt.table, combined_columns);
        current_context_.combined_schema = std::make_unique<Schema>(combined_columns);
    } else {
        current_context_.combined_schema = nullptr;
    }

    for (const auto& expr : stmt.select_list) {
        if (dynamic_cast<ast::StarExpression*>(expr.get())) {
            if (!current_context_.combined_schema) {
                throw BinderError("SELECT * with no table specified");
            }
            const auto& cols = current_context_.combined_schema->getColumns();
            for (size_t i = 0; i < cols.size(); ++i) {
                TypeId type = columnTypeToTypeId(cols[i].getType());
                auto col_val = std::make_unique<hamdb::ColumnValueExpression>(i);
                bound->select_list_.push_back(std::make_unique<BoundColumnRef>(std::move(col_val), type, "", cols[i].getName()));
            }
        } else {
            bound->select_list_.push_back(bindExpression(*expr));
        }
    }

    // Process GROUP BY
    for (const auto& group_expr : stmt.group_by) {
        bound->group_bys_.push_back(bindExpression(*group_expr));
    }

    // Validate GROUP BY semantics
    bool has_aggregate = false;
    for (const auto& expr : bound->select_list_) {
        if (expr->getBoundType() == BoundExpressionType::AGGREGATE) {
            has_aggregate = true;
            break;
        }
    }
    
    if (has_aggregate || !bound->group_bys_.empty()) {
        for (const auto& expr : bound->select_list_) {
            if (expr->getBoundType() == BoundExpressionType::AGGREGATE) continue;
            
            // It must be present in GROUP BY list
            bool found_in_group_by = false;
            for (const auto& gb : bound->group_bys_) {
                // simple check for column ref equality, ideally checking actual expression equality
                if (expr->getBoundType() == BoundExpressionType::COLUMN_REF &&
                    gb->getBoundType() == BoundExpressionType::COLUMN_REF) {
                    auto col1 = static_cast<const BoundColumnRef*>(expr.get());
                    auto col2 = static_cast<const BoundColumnRef*>(gb.get());
                    if (col1->getColumnName() == col2->getColumnName() &&
                        col1->getTableName() == col2->getTableName()) {
                        found_in_group_by = true;
                        break;
                    }
                }
            }
            if (!found_in_group_by && expr->getBoundType() != BoundExpressionType::CONSTANT && expr->getBoundType() != BoundExpressionType::PARAMETER) {
                throw BinderError("Selected column must be in GROUP BY clause or be an aggregate function");
            }
        }
    }

    if (stmt.where_clause) {
        bound->where_clause_ = bindExpression(*stmt.where_clause);
        if (bound->where_clause_->getType() != TypeId::Boolean) {
            throw BinderError("WHERE clause must be of boolean type");
        }
    }

    if (stmt.having_clause) {
        std::function<void(const ast::Expression*, bool)> validateHaving = [&](const ast::Expression* expr, bool in_agg) {
            if (!expr) return;
            if (auto e = dynamic_cast<const ast::AggregateExpression*>(expr)) {
                if (e->child) validateHaving(e->child.get(), true);
                return;
            }
            if (auto e = dynamic_cast<const ast::ColumnValueExpression*>(expr)) {
                if (in_agg) return;
                bool found = false;
                for (const auto& gb : stmt.group_by) {
                    if (auto gb_col = dynamic_cast<const ast::ColumnValueExpression*>(gb.get())) {
                        if (gb_col->column_name == e->column_name && gb_col->table_name == e->table_name) {
                            found = true;
                            break;
                        }
                    }
                }
                if (!found) {
                    throw BinderError("Non-grouped/non-aggregate columns rejected in HAVING");
                }
                return;
            }
            if (auto e = dynamic_cast<const ast::BinaryExpression*>(expr)) {
                validateHaving(e->left.get(), in_agg);
                validateHaving(e->right.get(), in_agg);
                return;
            }
            if (auto e = dynamic_cast<const ast::UnaryExpression*>(expr)) {
                validateHaving(e->child.get(), in_agg);
                return;
            }
        };
        validateHaving(stmt.having_clause.get(), false);

        bound->having_clause_ = bindExpression(*stmt.having_clause);
        if (bound->having_clause_->getType() != TypeId::Boolean) {
            throw BinderError("HAVING clause must be of boolean type");
        }
    }

    for (const auto& order : stmt.order_by) {
        bound->order_by_.emplace_back(bindExpression(*order.first), order.second);
    }
    
    if (stmt.limit) bound->limit_ = bindExpression(*stmt.limit);
    if (stmt.offset) bound->offset_ = bindExpression(*stmt.offset);

    // clear context
    current_context_ = Context();
    return bound;
}

std::unique_ptr<BoundInsertStatement> Binder::bindInsert(const ast::InsertStatement& stmt) {
    auto bound = std::make_unique<BoundInsertStatement>();
    TableInfo* table_info = nullptr;
    if (catalog_->getTable(stmt.table_name_, table_info) != Status::Ok) {
        throw BinderError("Unknown table: " + stmt.table_name_);
    }
    bound->table_name_ = stmt.table_name_;

    for (const auto& row : stmt.values) {
        std::vector<std::unique_ptr<BoundExpression>> bound_row;
        if (row.size() != table_info->getSchema().getColumnCount()) {
            throw BinderError("Insert values count mismatch");
        }
        for (size_t i = 0; i < row.size(); ++i) {
            auto bound_expr = bindExpression(*row[i]);
            TypeId expected = columnTypeToTypeId(table_info->getSchema().getColumn(i).getType());
            if (bound_expr->getBoundType() == BoundExpressionType::PARAMETER) {
                auto param = static_cast<BoundParameter*>(bound_expr.get());
                param->setType(expected);
                parameter_types_[param->getIndex()] = expected;
            } else if (bound_expr->getType() != expected && bound_expr->getType() != TypeId::Null) {
                throw BinderError("Type mismatch in INSERT statement");
            }
            bound_row.push_back(std::move(bound_expr));
        }
        bound->values_.push_back(std::move(bound_row));
    }
    return bound;
}

std::unique_ptr<BoundUpdateStatement> Binder::bindUpdate(const ast::UpdateStatement& stmt) {
    auto bound = std::make_unique<BoundUpdateStatement>();
    TableInfo* table_info = nullptr;
    if (catalog_->getTable(stmt.table_name_, table_info) != Status::Ok) {
        throw BinderError("Unknown table: " + stmt.table_name_);
    }
    bound->table_name_ = stmt.table_name_;
    
    current_context_.tables.clear();
    current_context_.tables.push_back({stmt.table_name_, "", &table_info->getSchema(), 0});
    std::vector<Column> cols;
    for (uint32_t i = 0; i < table_info->getSchema().getColumnCount(); ++i) {
        cols.push_back(table_info->getSchema().getColumn(i));
    }
    current_context_.combined_schema = std::make_unique<Schema>(cols);

    for (const auto& set_clause : stmt.set_clauses) {
        auto bound_expr = bindExpression(*set_clause.second);
        bool found = false;
        const auto& schema_cols = table_info->getSchema().getColumns();
        for (size_t i = 0; i < schema_cols.size(); ++i) {
            if (schema_cols[i].getName() == set_clause.first) {
                found = true;
                TypeId expected = columnTypeToTypeId(schema_cols[i].getType());
                if (bound_expr->getBoundType() == BoundExpressionType::PARAMETER) {
                    auto param = static_cast<BoundParameter*>(bound_expr.get());
                    param->setType(expected);
                    parameter_types_[param->getIndex()] = expected;
                } else if (bound_expr->getType() != expected && bound_expr->getType() != TypeId::Null) {
                    throw BinderError("Type mismatch in UPDATE statement");
                }
                break;
            }
        }
        if (!found) {
            throw BinderError("Unknown column in SET clause: " + set_clause.first);
        }
        bound->set_clauses_.emplace_back(set_clause.first, std::move(bound_expr));
    }

    if (stmt.where_clause) {
        bound->where_clause_ = bindExpression(*stmt.where_clause);
        if (bound->where_clause_->getType() != TypeId::Boolean) {
            throw BinderError("WHERE clause must be of boolean type");
        }
    }

    current_context_ = Context();
    return bound;
}

std::unique_ptr<BoundDeleteStatement> Binder::bindDelete(const ast::DeleteStatement& stmt) {
    auto bound = std::make_unique<BoundDeleteStatement>();
    TableInfo* table_info = nullptr;
    if (catalog_->getTable(stmt.table_name_, table_info) != Status::Ok) {
        throw BinderError("Unknown table: " + stmt.table_name_);
    }
    bound->table_name_ = stmt.table_name_;
    
    current_context_.tables.clear();
    current_context_.tables.push_back({stmt.table_name_, "", &table_info->getSchema(), 0});
    std::vector<Column> cols;
    for (uint32_t i = 0; i < table_info->getSchema().getColumnCount(); ++i) {
        cols.push_back(table_info->getSchema().getColumn(i));
    }
    current_context_.combined_schema = std::make_unique<Schema>(cols);

    if (stmt.where_clause) {
        bound->where_clause_ = bindExpression(*stmt.where_clause);
        if (bound->where_clause_->getType() != TypeId::Boolean) {
            throw BinderError("WHERE clause must be of boolean type");
        }
    }

    current_context_ = Context();
    return bound;
}

std::unique_ptr<BoundValuesStatement> Binder::bindValues(const ast::ValuesStatement& stmt) {
    auto bound = std::make_unique<BoundValuesStatement>();
    for (const auto& row : stmt.values) {
        std::vector<std::unique_ptr<BoundExpression>> bound_row;
        for (const auto& expr : row) {
            bound_row.push_back(bindExpression(*expr));
        }
        bound->values_.push_back(std::move(bound_row));
    }
    return bound;
}

std::unique_ptr<BoundExpression> Binder::bindExpression(const ast::Expression& expr) {
    if (auto e = dynamic_cast<const ast::ParameterExpression*>(&expr)) return bindParameter(*e);
    if (auto e = dynamic_cast<const ast::ConstantExpression*>(&expr)) return bindConstant(*e);
    if (auto e = dynamic_cast<const ast::ColumnValueExpression*>(&expr)) return bindColumnValue(*e);
    if (auto e = dynamic_cast<const ast::BinaryExpression*>(&expr)) return bindBinary(*e);
    if (auto e = dynamic_cast<const ast::UnaryExpression*>(&expr)) return bindUnary(*e);
    if (auto e = dynamic_cast<const ast::AggregateExpression*>(&expr)) return bindAggregate(*e);
    throw BinderError("Unknown expression type");
}

std::unique_ptr<BoundExpression> Binder::bindParameter(const ast::ParameterExpression& /*expr*/) {
    size_t index = parameter_types_.size();
    parameter_types_.push_back(TypeId::Null); // Will be inferred
    auto executor_expr = std::make_unique<hamdb::ParameterExpression>(index);
    return std::make_unique<BoundParameter>(std::move(executor_expr), TypeId::Null, index);
}

std::unique_ptr<BoundExpression> Binder::bindConstant(const ast::ConstantExpression& expr) {
    Value val;
    TypeId type;
    switch (expr.type) {
        case ast::ConstantExpression::Type::Integer:
            val = Value(std::stoi(expr.value));
            type = TypeId::Integer;
            break;
        case ast::ConstantExpression::Type::String:
            val = Value(expr.value);
            type = TypeId::Varchar;
            break;
        case ast::ConstantExpression::Type::Boolean:
            val = Value(expr.value == "true");
            type = TypeId::Boolean;
            break;
        case ast::ConstantExpression::Type::Null:
            val = Value();
            type = TypeId::Null;
            break;
        default:
            throw BinderError("Unknown constant type");
    }
    auto executor_expr = std::make_unique<hamdb::ConstantExpression>(val);
    return std::make_unique<BoundConstant>(std::move(executor_expr), type);
}

std::unique_ptr<BoundExpression> Binder::bindAggregate(const ast::AggregateExpression& expr) {
    std::unique_ptr<BoundExpression> child;
    TypeId child_type = TypeId::Integer;
    if (expr.child) {
        child = bindExpression(*expr.child);
        child_type = child->getType();
    }
    
    hamdb::AggregateType agg_type;
    TypeId result_type = TypeId::Integer;
    
    switch (expr.type) {
        case ast::AggregateExpression::Type::CountStar:
            agg_type = hamdb::AggregateType::CountStar;
            break;
        case ast::AggregateExpression::Type::Count:
            agg_type = hamdb::AggregateType::Count;
            break;
        case ast::AggregateExpression::Type::Sum:
            agg_type = hamdb::AggregateType::Sum;
            result_type = child_type;
            break;
        case ast::AggregateExpression::Type::Min:
            agg_type = hamdb::AggregateType::Min;
            result_type = child_type;
            break;
        case ast::AggregateExpression::Type::Max:
            agg_type = hamdb::AggregateType::Max;
            result_type = child_type;
            break;
        case ast::AggregateExpression::Type::Avg:
            agg_type = hamdb::AggregateType::Avg;
            result_type = child_type;
            break;
    }
    
    return std::make_unique<BoundAggregate>(child ? child->takeExpr() : nullptr, result_type, agg_type);
}

std::unique_ptr<BoundExpression> Binder::bindColumnValue(const ast::ColumnValueExpression& expr) {
    if (!current_context_.combined_schema) {
        throw BinderError("Column reference without a table");
    }

    int found_idx = -1;
    std::string found_table_name;
    TypeId type;

    if (!expr.table_name.empty()) {
        bool table_found = false;
        for (const auto& tc : current_context_.tables) {
            if (tc.table_name == expr.table_name || tc.table_alias == expr.table_name) {
                table_found = true;
                const auto& cols = tc.schema->getColumns();
                for (size_t i = 0; i < cols.size(); ++i) {
                    if (cols[i].getName() == expr.column_name) {
                        if (found_idx != -1) {
                            throw BinderError("Ambiguous column: " + expr.column_name);
                        }
                        found_idx = tc.column_offset + i;
                        found_table_name = tc.table_name;
                        type = columnTypeToTypeId(cols[i].getType());
                    }
                }
            }
        }
        if (!table_found) {
            throw BinderError("Unknown table or alias: " + expr.table_name);
        }
    } else {
        const auto& cols = current_context_.combined_schema->getColumns();
        for (size_t i = 0; i < cols.size(); ++i) {
            if (cols[i].getName() == expr.column_name) {
                if (found_idx != -1) {
                    throw BinderError("Ambiguous column: " + expr.column_name);
                }
                found_idx = i;
                type = columnTypeToTypeId(cols[i].getType());
            }
        }
    }

    if (found_idx == -1) {
        throw BinderError("Unknown column: " + expr.column_name);
    }

    auto executor_expr = std::make_unique<hamdb::ColumnValueExpression>(found_idx);
    return std::make_unique<BoundColumnRef>(std::move(executor_expr), type, found_table_name, expr.column_name);
}

std::unique_ptr<BoundExpression> Binder::bindBinary(const ast::BinaryExpression& expr) {
    auto left = bindExpression(*expr.left);
    auto right = bindExpression(*expr.right);

    // Type inference for parameters
    if (left->getBoundType() == BoundExpressionType::PARAMETER && right->getBoundType() != BoundExpressionType::PARAMETER) {
        auto param = static_cast<BoundParameter*>(left.get());
        param->setType(right->getType());
        parameter_types_[param->getIndex()] = right->getType();
    } else if (right->getBoundType() == BoundExpressionType::PARAMETER && left->getBoundType() != BoundExpressionType::PARAMETER) {
        auto param = static_cast<BoundParameter*>(right.get());
        param->setType(left->getType());
        parameter_types_[param->getIndex()] = left->getType();
    }

    // Basic type checking
    if (left->getType() != right->getType() && left->getType() != TypeId::Null && right->getType() != TypeId::Null) {
        throw BinderError("Type mismatch in binary expression");
    }

    switch (expr.op) {
        case ast::BinaryExpression::Op::Add:
        case ast::BinaryExpression::Op::Subtract:
        case ast::BinaryExpression::Op::Multiply:
        case ast::BinaryExpression::Op::Divide:
        case ast::BinaryExpression::Op::Modulo: {
            ArithmeticType op;
            if (expr.op == ast::BinaryExpression::Op::Add) op = ArithmeticType::Add;
            else if (expr.op == ast::BinaryExpression::Op::Subtract) op = ArithmeticType::Subtract;
            else if (expr.op == ast::BinaryExpression::Op::Multiply) op = ArithmeticType::Multiply;
            else if (expr.op == ast::BinaryExpression::Op::Divide) op = ArithmeticType::Divide;
            else if (expr.op == ast::BinaryExpression::Op::Modulo) op = ArithmeticType::Modulo;
            else throw BinderError("Unknown arithmetic operation");

            auto e = std::make_unique<hamdb::ArithmeticExpression>(op, left->takeExpr(), right->takeExpr());
            return std::make_unique<BoundArithmetic>(std::move(e), left->getType());
        }
        case ast::BinaryExpression::Op::Equals:
        case ast::BinaryExpression::Op::NotEquals:
        case ast::BinaryExpression::Op::Less:
        case ast::BinaryExpression::Op::LessEquals:
        case ast::BinaryExpression::Op::Greater:
        case ast::BinaryExpression::Op::GreaterEquals: {
            ComparisonType op;
            if (expr.op == ast::BinaryExpression::Op::Equals) op = ComparisonType::Equal;
            else if (expr.op == ast::BinaryExpression::Op::NotEquals) op = ComparisonType::NotEqual;
            else if (expr.op == ast::BinaryExpression::Op::Less) op = ComparisonType::LessThan;
            else if (expr.op == ast::BinaryExpression::Op::LessEquals) op = ComparisonType::LessThanOrEqual;
            else if (expr.op == ast::BinaryExpression::Op::Greater) op = ComparisonType::GreaterThan;
            else op = ComparisonType::GreaterThanOrEqual;

            auto e = std::make_unique<hamdb::ComparisonExpression>(op, left->takeExpr(), right->takeExpr());
            return std::make_unique<BoundComparison>(std::move(e), TypeId::Boolean);
        }
        case ast::BinaryExpression::Op::And:
        case ast::BinaryExpression::Op::Or: {
            LogicalType op = (expr.op == ast::BinaryExpression::Op::And) ? LogicalType::And : LogicalType::Or;
            auto e = std::make_unique<hamdb::LogicalExpression>(op, left->takeExpr(), right->takeExpr());
            return std::make_unique<BoundLogical>(std::move(e), TypeId::Boolean);
        }
        default:
            throw BinderError("Unknown binary operator");
    }
}

std::unique_ptr<BoundExpression> Binder::bindUnary(const ast::UnaryExpression& expr) {
    auto child = bindExpression(*expr.child);
    if (expr.op == ast::UnaryExpression::Op::Not) {
        if (child->getType() != TypeId::Boolean && child->getType() != TypeId::Null) {
            throw BinderError("NOT requires a boolean expression");
        }
        auto e = std::make_unique<hamdb::LogicalExpression>(LogicalType::Not, child->takeExpr());
        return std::make_unique<BoundLogical>(std::move(e), TypeId::Boolean);
    }
    if (expr.op == ast::UnaryExpression::Op::Minus) {
        if (child->getType() != TypeId::Integer && child->getType() != TypeId::Null) {
            throw BinderError("Unary minus requires an integer expression");
        }
        auto zero = std::make_unique<hamdb::ConstantExpression>(Value(0));
        auto e = std::make_unique<hamdb::ArithmeticExpression>(ArithmeticType::Subtract, std::move(zero), child->takeExpr());
        return std::make_unique<BoundArithmetic>(std::move(e), child->getType());
    }
    if (expr.op == ast::UnaryExpression::Op::Plus) {
        if (child->getType() != TypeId::Integer && child->getType() != TypeId::Null) {
            throw BinderError("Unary plus requires an integer expression");
        }
        return child; // just pass through
    }
    throw BinderError("Unknown unary operator");
}

} // namespace hamdb::binder
