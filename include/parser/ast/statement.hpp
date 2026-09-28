#pragma once
#include "parser/ast/expression.hpp"
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <utility>

namespace hamdb::ast {


class TableReference : public ASTNode {
public:
    virtual ~TableReference() = default;
    virtual std::string toString() const = 0;
};

class BaseTableReference : public TableReference {
public:
    std::string table_name_;
    std::string table_alias_;
    std::string toString() const override;
};

class JoinTableReference : public TableReference {
public:
    std::unique_ptr<TableReference> left_;
    std::unique_ptr<TableReference> right_;
    std::unique_ptr<Expression> condition_;
    std::string toString() const override;
};

class Statement : public ASTNode {
public:
    virtual ~Statement() = default;
};

class SelectStatement : public Statement {
public:
    std::vector<std::unique_ptr<Expression>> select_list;
    std::unique_ptr<TableReference> table;
    std::unique_ptr<Expression> where_clause;
    
    // Group By
    std::vector<std::unique_ptr<Expression>> group_by;
    
    // Order By
    std::vector<std::pair<std::unique_ptr<Expression>, bool>> order_by; // true for ASC, false for DESC
    
    std::unique_ptr<Expression> limit;
    std::unique_ptr<Expression> offset;

    std::string toString() const override;
};

class InsertStatement : public Statement {
public:
    std::string table_name_;
    std::vector<std::vector<std::unique_ptr<Expression>>> values;

    std::string toString() const override;
};

class UpdateStatement : public Statement {
public:
    std::string table_name_;
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> set_clauses;
    std::unique_ptr<Expression> where_clause;

    std::string toString() const override;
};

class DeleteStatement : public Statement {
public:
    std::string table_name_;
    std::unique_ptr<Expression> where_clause;

    std::string toString() const override;
};

class ValuesStatement : public Statement {
public:
    std::vector<std::vector<std::unique_ptr<Expression>>> values;

    std::string toString() const override;
};

class ExplainStatement : public Statement {
public:
    std::unique_ptr<Statement> statement;
    bool analyze{false};

    std::string toString() const override;
};

class PrepareStatement : public Statement {
public:
    std::string name;
    std::unique_ptr<Statement> query;

    std::string toString() const override;
};

class ExecuteStatement : public Statement {
public:
    std::string name;
    std::vector<std::unique_ptr<Expression>> parameters;

    std::string toString() const override;
};

class DeallocateStatement : public Statement {
public:
    std::string name;

    std::string toString() const override;
};

} // namespace hamdb::ast
