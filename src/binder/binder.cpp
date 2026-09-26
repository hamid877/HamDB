#include "binder/binder.hpp"
#include <algorithm>
#include <cctype>

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

std::unique_ptr<BoundSelectStatement> Binder::bindSelect(const ast::SelectStatement& stmt) {
    auto bound = std::make_unique<BoundSelectStatement>();
    
    if (!stmt.table_name.empty()) {
        TableInfo* table_info = nullptr;
        if (catalog_->getTable(stmt.table_name, table_info) != Status::Ok) {
            throw BinderError("Unknown table: " + stmt.table_name);
        }
        current_context_.table_name = stmt.table_name;
        current_context_.table_alias = stmt.table_alias;
        current_context_.schema = &table_info->getSchema();
        bound->table_name_ = stmt.table_name;
        bound->table_alias_ = stmt.table_alias;
    } else {
        current_context_.schema = nullptr;
    }

    for (const auto& expr : stmt.select_list) {
        if (dynamic_cast<ast::StarExpression*>(expr.get())) {
            if (!current_context_.schema) {
                throw BinderError("SELECT * with no table specified");
            }
            const auto& cols = current_context_.schema->getColumns();
            for (size_t i = 0; i < cols.size(); ++i) {
                TypeId type = columnTypeToTypeId(cols[i].getType());
                auto col_val = std::make_unique<hamdb::ColumnValueExpression>(i);
                bound->select_list_.push_back(std::make_unique<BoundColumnRef>(std::move(col_val), type, current_context_.table_name, cols[i].getName()));
            }
        } else {
            bound->select_list_.push_back(bindExpression(*expr));
        }
    }

    if (stmt.where_clause) {
        bound->where_clause_ = bindExpression(*stmt.where_clause);
        if (bound->where_clause_->getType() != TypeId::Boolean) {
            throw BinderError("WHERE clause must be of boolean type");
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
    if (catalog_->getTable(stmt.table_name, table_info) != Status::Ok) {
        throw BinderError("Unknown table: " + stmt.table_name);
    }
    bound->table_name_ = stmt.table_name;

    for (const auto& row : stmt.values) {
        std::vector<std::unique_ptr<BoundExpression>> bound_row;
        if (row.size() != table_info->getSchema().getColumnCount()) {
            throw BinderError("Insert values count mismatch");
        }
        for (size_t i = 0; i < row.size(); ++i) {
            auto bound_expr = bindExpression(*row[i]);
            TypeId expected = columnTypeToTypeId(table_info->getSchema().getColumn(i).getType());
            if (bound_expr->getType() != expected && bound_expr->getType() != TypeId::Null) {
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
    if (catalog_->getTable(stmt.table_name, table_info) != Status::Ok) {
        throw BinderError("Unknown table: " + stmt.table_name);
    }
    bound->table_name_ = stmt.table_name;
    current_context_.table_name = stmt.table_name;
    current_context_.table_alias = "";
    current_context_.schema = &table_info->getSchema();

    for (const auto& set_clause : stmt.set_clauses) {
        auto bound_expr = bindExpression(*set_clause.second);
        bool found = false;
        const auto& cols = table_info->getSchema().getColumns();
        for (size_t i = 0; i < cols.size(); ++i) {
            if (cols[i].getName() == set_clause.first) {
                found = true;
                TypeId expected = columnTypeToTypeId(cols[i].getType());
                if (bound_expr->getType() != expected && bound_expr->getType() != TypeId::Null) {
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
    if (catalog_->getTable(stmt.table_name, table_info) != Status::Ok) {
        throw BinderError("Unknown table: " + stmt.table_name);
    }
    bound->table_name_ = stmt.table_name;
    current_context_.table_name = stmt.table_name;
    current_context_.table_alias = "";
    current_context_.schema = &table_info->getSchema();

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
    if (auto e = dynamic_cast<const ast::ConstantExpression*>(&expr)) return bindConstant(*e);
    if (auto e = dynamic_cast<const ast::ColumnValueExpression*>(&expr)) return bindColumnValue(*e);
    if (auto e = dynamic_cast<const ast::BinaryExpression*>(&expr)) return bindBinary(*e);
    if (auto e = dynamic_cast<const ast::UnaryExpression*>(&expr)) return bindUnary(*e);
    throw BinderError("Unknown expression type");
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

std::unique_ptr<BoundExpression> Binder::bindColumnValue(const ast::ColumnValueExpression& expr) {
    if (!current_context_.schema) {
        throw BinderError("Column reference without a table");
    }

    // Check table match
    if (!expr.table_name.empty()) {
        if (expr.table_name != current_context_.table_name && expr.table_name != current_context_.table_alias) {
            throw BinderError("Unknown table or alias: " + expr.table_name);
        }
    }

    const auto& cols = current_context_.schema->getColumns();
    int found_idx = -1;
    for (size_t i = 0; i < cols.size(); ++i) {
        if (cols[i].getName() == expr.column_name) {
            if (found_idx != -1) {
                throw BinderError("Ambiguous column: " + expr.column_name);
            }
            found_idx = i;
        }
    }

    if (found_idx == -1) {
        throw BinderError("Unknown column: " + expr.column_name);
    }

    TypeId type = columnTypeToTypeId(cols[found_idx].getType());
    auto executor_expr = std::make_unique<hamdb::ColumnValueExpression>(found_idx);
    return std::make_unique<BoundColumnRef>(std::move(executor_expr), type, current_context_.table_name, expr.column_name);
}

std::unique_ptr<BoundExpression> Binder::bindBinary(const ast::BinaryExpression& expr) {
    auto left = bindExpression(*expr.left);
    auto right = bindExpression(*expr.right);

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
            else throw BinderError("Modulo not supported in ArithmeticType");

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
