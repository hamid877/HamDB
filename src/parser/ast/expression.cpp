#include "parser/ast/expression.hpp"

namespace hamdb::ast {

std::string ConstantExpression::toString() const {
    if (type == Type::String) {
        return "'" + value + "'";
    }
    return value;
}

std::string ColumnValueExpression::toString() const {
    if (table_name.empty()) {
        return column_name;
    }
    return table_name + "." + column_name;
}

std::string BinaryExpression::toString() const {
    std::string op_str;
    switch (op) {
        case Op::Add: op_str = "+"; break;
        case Op::Subtract: op_str = "-"; break;
        case Op::Multiply: op_str = "*"; break;
        case Op::Divide: op_str = "/"; break;
        case Op::Modulo: op_str = "%"; break;
        case Op::Equals: op_str = "="; break;
        case Op::NotEquals: op_str = "!="; break;
        case Op::Less: op_str = "<"; break;
        case Op::LessEquals: op_str = "<="; break;
        case Op::Greater: op_str = ">"; break;
        case Op::GreaterEquals: op_str = ">="; break;
        case Op::And: op_str = "AND"; break;
        case Op::Or: op_str = "OR"; break;
    }
    return "(" + left->toString() + " " + op_str + " " + right->toString() + ")";
}

std::string UnaryExpression::toString() const {
    std::string op_str;
    switch (op) {
        case Op::Not: op_str = "NOT "; break;
        case Op::Plus: op_str = "+"; break;
        case Op::Minus: op_str = "-"; break;
    }
    return "(" + op_str + child->toString() + ")";
}

} // namespace hamdb::ast
