#pragma once

#include <string>
#include <string_view>
#include <ostream>
#include <cstdint>

namespace hamdb {

/**
 * @brief Represents the types of tokens produced by the Lexer.
 */
enum class TokenType {
    // Keywords
    Select, Insert, Update, Delete, From, Where,
    And, Or, Not, Limit, Offset, Order, By, Asc, Desc,
    Values, Into, Set, Create, Table,
    Int, Boolean, Varchar, Primary, Key,

    // Identifiers
    Identifier,

    // Literals
    Integer, String, BoolLiteral, NullLiteral,

    // Arithmetic
    Plus, Minus, Asterisk, Slash,

    // Comparison
    Equals, NotEquals, Less, LessEquals, Greater, GreaterEquals,

    // Delimiters
    Comma, LeftParen, RightParen, Semicolon, Dot,

    // Special
    Eof, Invalid
};

/**
 * @brief Represents a single token extracted from the source string.
 */
struct Token {
    TokenType type;
    std::string lexeme;
    std::size_t line;
    std::size_t column;

    Token(TokenType type, std::string lexeme, std::size_t line, std::size_t column)
        : type(type), lexeme(std::move(lexeme)), line(line), column(column) {}

    bool operator==(const Token& other) const = default;
};

/**
 * @brief Converts a TokenType to its string representation.
 */
std::string_view tokenTypeToString(TokenType type);

/**
 * @brief Output stream operator for Token.
 */
std::ostream& operator<<(std::ostream& os, const Token& token);

} // namespace hamdb
