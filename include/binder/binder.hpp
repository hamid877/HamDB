#pragma once

#include "binder/bound_statement.hpp"
#include "parser/ast/statement.hpp"
#include "catalog/catalog_manager.hpp"
#include "catalog/schema.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <stdexcept>

namespace hamdb::binder {

class BinderError : public std::runtime_error {
public:
    explicit BinderError(const std::string& message) : std::runtime_error(message) {}
};

class Binder {
public:
    explicit Binder(CatalogManager* catalog) : catalog_(catalog) {}

    std::unique_ptr<BoundStatement> bind(const ast::Statement& stmt);

private:
    CatalogManager* catalog_;
    
    // Binding context for the current statement
    struct Context {
        std::string table_name;
        std::string table_alias;
        const Schema* schema{nullptr};
    };
    Context current_context_;

    std::unique_ptr<BoundSelectStatement> bindSelect(const ast::SelectStatement& stmt);
    std::unique_ptr<BoundInsertStatement> bindInsert(const ast::InsertStatement& stmt);
    std::unique_ptr<BoundUpdateStatement> bindUpdate(const ast::UpdateStatement& stmt);
    std::unique_ptr<BoundDeleteStatement> bindDelete(const ast::DeleteStatement& stmt);
    std::unique_ptr<BoundValuesStatement> bindValues(const ast::ValuesStatement& stmt);

    std::unique_ptr<BoundExpression> bindExpression(const ast::Expression& expr);
    
    std::unique_ptr<BoundExpression> bindConstant(const ast::ConstantExpression& expr);
    std::unique_ptr<BoundExpression> bindColumnValue(const ast::ColumnValueExpression& expr);
    std::unique_ptr<BoundExpression> bindBinary(const ast::BinaryExpression& expr);
    std::unique_ptr<BoundExpression> bindUnary(const ast::UnaryExpression& expr);

    TypeId columnTypeToTypeId(ColumnType type);
};

} // namespace hamdb::binder
