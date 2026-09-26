#pragma once
#include "parser/ast/ast_node.hpp"
#include <memory>
#include <string>
#include <vector>

namespace hamdb::ast {

class Expression : public ASTNode {
public:
    virtual ~Expression() = default;
};

class ConstantExpression : public Expression {
public:
    enum class Type { Integer, String, Boolean, Null };
    Type type;
    std::string value;

    ConstantExpression(Type type, std::string value) : type(type), value(std::move(value)) {}
    std::string toString() const override;
};

class ColumnValueExpression : public Expression {
public:
    std::string column_name;
    std::string table_name;

    explicit ColumnValueExpression(std::string column_name, std::string table_name = "")
        : column_name(std::move(column_name)), table_name(std::move(table_name)) {}
    std::string toString() const override;
};

class BinaryExpression : public Expression {
public:
    enum class Op { Add, Subtract, Multiply, Divide, Modulo, Equals, NotEquals, Less, LessEquals, Greater, GreaterEquals, And, Or };
    Op op;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;

    BinaryExpression(Op op, std::unique_ptr<Expression> left, std::unique_ptr<Expression> right)
        : op(op), left(std::move(left)), right(std::move(right)) {}
    std::string toString() const override;
};

class UnaryExpression : public Expression {
public:
    enum class Op { Not, Plus, Minus };
    Op op;
    std::unique_ptr<Expression> child;

    UnaryExpression(Op op, std::unique_ptr<Expression> child)
        : op(op), child(std::move(child)) {}
    std::string toString() const override;
};

} // namespace hamdb::ast
