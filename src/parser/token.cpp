#include "parser/token.hpp"

namespace hamdb {

std::string_view tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::Select: return "Select";
        case TokenType::Insert: return "Insert";
        case TokenType::Update: return "Update";
        case TokenType::Delete: return "Delete";
        case TokenType::From: return "From";
        case TokenType::Where: return "Where";
        case TokenType::And: return "And";
        case TokenType::Or: return "Or";
        case TokenType::Not: return "Not";
        case TokenType::Limit: return "Limit";
        case TokenType::Offset: return "Offset";
        case TokenType::Order: return "Order";
        case TokenType::By: return "By";
        case TokenType::Asc: return "Asc";
        case TokenType::Desc: return "Desc";
        case TokenType::Values: return "Values";
        case TokenType::Into: return "Into";
        case TokenType::Set: return "Set";
        case TokenType::Create: return "Create";
        case TokenType::Table: return "Table";
        case TokenType::Int: return "Int";
        case TokenType::Boolean: return "Boolean";
        case TokenType::Varchar: return "Varchar";
        case TokenType::Primary: return "Primary";
        case TokenType::Key: return "Key";
        case TokenType::Identifier: return "Identifier";
        case TokenType::Integer: return "Integer";
        case TokenType::String: return "String";
        case TokenType::BoolLiteral: return "BoolLiteral";
        case TokenType::NullLiteral: return "NullLiteral";
        case TokenType::Plus: return "Plus";
        case TokenType::Minus: return "Minus";
        case TokenType::Asterisk: return "Asterisk";
        case TokenType::Slash: return "Slash";
        case TokenType::Equals: return "Equals";
        case TokenType::NotEquals: return "NotEquals";
        case TokenType::Less: return "Less";
        case TokenType::LessEquals: return "LessEquals";
        case TokenType::Greater: return "Greater";
        case TokenType::GreaterEquals: return "GreaterEquals";
        case TokenType::Comma: return "Comma";
        case TokenType::LeftParen: return "LeftParen";
        case TokenType::RightParen: return "RightParen";
        case TokenType::Semicolon: return "Semicolon";
        case TokenType::Dot: return "Dot";
        case TokenType::Eof: return "Eof";
        case TokenType::Invalid: return "Invalid";
        default: return "Unknown";
    }
}

std::ostream& operator<<(std::ostream& os, const Token& token) {
    os << "Token(" << tokenTypeToString(token.type) << ", '" << token.lexeme 
       << "', line=" << token.line << ", col=" << token.column << ")";
    return os;
}

} // namespace hamdb
