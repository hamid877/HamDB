#pragma once

#include "parser/token.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace hamdb {

/**
 * @brief Performs lexical analysis on SQL strings.
 */
class Lexer {
public:
    /**
     * @brief Constructs a lexer from the given source string.
     * @param source The SQL source string.
     */
    explicit Lexer(std::string_view source);

    /**
     * @brief Tokenizes the entire source string.
     * @return A vector of tokens ending with an Eof token.
     */
    std::vector<Token> tokenize();

    /**
     * @brief Returns the next token from the source string.
     * @return The next token. Returns an Eof token when the end is reached.
     */
    Token nextToken();

private:
    char peek() const;
    char peekNext() const;
    char advance();
    bool isAtEnd() const;
    void skipWhitespace();

    Token number();
    Token string();
    Token identifierOrKeyword();
    Token makeToken(TokenType type);
    Token makeToken(TokenType type, std::string lexeme);
    Token makeErrorToken(std::string message);

    std::string_view source_;
    std::size_t start_{0};
    std::size_t current_{0};
    std::size_t line_{1};
    std::size_t column_{1};
    std::size_t start_column_{1};
};

} // namespace hamdb
