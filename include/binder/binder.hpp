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
    std::unique_ptr<BoundExpression> bindExpression(const ast::Expression& expr);
    const std::vector<TypeId>& getParameterTypes() const { return parameter_types_; }

private:
    CatalogManager* catalog_;
    
    // Binding context for the current statement
    struct TableContext {
        std::string table_name;
        std::string table_alias;
        const Schema* schema;
        size_t column_offset;
    };
    struct Context {
        std::vector<TableContext> tables;
        std::unique_ptr<Schema> combined_schema;
    };
    Context current_context_;

    std::unique_ptr<BoundTableReference> bindTableReference(const ast::TableReference& ref, std::vector<Column>& combined_columns);
    std::unique_ptr<BoundSelectStatement> bindSelect(const ast::SelectStatement& stmt);
    std::unique_ptr<BoundInsertStatement> bindInsert(const ast::InsertStatement& stmt);
    std::unique_ptr<BoundUpdateStatement> bindUpdate(const ast::UpdateStatement& stmt);
    std::unique_ptr<BoundDeleteStatement> bindDelete(const ast::DeleteStatement& stmt);
    std::unique_ptr<BoundValuesStatement> bindValues(const ast::ValuesStatement& stmt);
    
    
    std::unique_ptr<BoundExpression> bindParameter(const ast::ParameterExpression& expr);
    std::unique_ptr<BoundExpression> bindConstant(const ast::ConstantExpression& expr);
    std::unique_ptr<BoundExpression> bindColumnValue(const ast::ColumnValueExpression& expr);
    std::unique_ptr<BoundExpression> bindAggregate(const ast::AggregateExpression& expr);
    std::unique_ptr<BoundExpression> bindBinary(const ast::BinaryExpression& expr);
    std::unique_ptr<BoundExpression> bindUnary(const ast::UnaryExpression& expr);

    TypeId columnTypeToTypeId(ColumnType type);

    std::vector<TypeId> parameter_types_;
};

} // namespace hamdb::binder
