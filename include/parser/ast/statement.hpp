#pragma once
#include "parser/ast/expression.hpp"
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <utility>

namespace hamdb::ast {

class Statement : public ASTNode {
public:
    virtual ~Statement() = default;
};

class SelectStatement : public Statement {
public:
    std::vector<std::unique_ptr<Expression>> select_list;
    std::string table_name;
    std::unique_ptr<Expression> where_clause;
    
    // Order By
    std::vector<std::pair<std::unique_ptr<Expression>, bool>> order_by; // true for ASC, false for DESC
    
    std::unique_ptr<Expression> limit;
    std::unique_ptr<Expression> offset;

    std::string toString() const override;
};

class InsertStatement : public Statement {
public:
    std::string table_name;
    std::vector<std::vector<std::unique_ptr<Expression>>> values;

    std::string toString() const override;
};

class UpdateStatement : public Statement {
public:
    std::string table_name;
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> set_clauses;
    std::unique_ptr<Expression> where_clause;

    std::string toString() const override;
};

class DeleteStatement : public Statement {
public:
    std::string table_name;
    std::unique_ptr<Expression> where_clause;

    std::string toString() const override;
};

class ValuesStatement : public Statement {
public:
    std::vector<std::vector<std::unique_ptr<Expression>>> values;

    std::string toString() const override;
};

} // namespace hamdb::ast
