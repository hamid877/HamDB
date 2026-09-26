#pragma once
#include "parser/lexer.hpp"
#include "parser/ast/statement.hpp"
#include "parser/ast/expression.hpp"
#include <memory>
#include <string>
#include <stdexcept>

namespace hamdb {

class ParserError : public std::runtime_error {
public:
    ParserError(const std::string& message, std::size_t line, std::size_t column)
        : std::runtime_error(message), line_(line), column_(column) {}

    std::size_t line() const { return line_; }
    std::size_t column() const { return column_; }

private:
    std::size_t line_;
    std::size_t column_;
};

class Parser {
public:
    explicit Parser(std::string_view source);

    std::unique_ptr<ast::Statement> parseStatement();
    std::unique_ptr<ast::Expression> parseExpression();

private:
    Lexer lexer_;
    Token current_token_;
    Token previous_token_;

    void advance();
    bool match(TokenType type);
    bool check(TokenType type) const;
    void consume(TokenType type, const std::string& message);
    [[noreturn]] void error(const Token& token, const std::string& message);

    // Expression parsing
    std::unique_ptr<ast::Expression> parseOr();
    std::unique_ptr<ast::Expression> parseAnd();
    std::unique_ptr<ast::Expression> parseComparison();
    std::unique_ptr<ast::Expression> parseTerm(); // +, -
    std::unique_ptr<ast::Expression> parseFactor(); // *, /, %
    std::unique_ptr<ast::Expression> parseUnary();
    std::unique_ptr<ast::Expression> parsePrimary();

public:
    // Statement parsing
    std::unique_ptr<ast::Statement> parseSelect();
    std::unique_ptr<ast::Statement> parseInsert();
    std::unique_ptr<ast::Statement> parseUpdate();
    std::unique_ptr<ast::Statement> parseDelete();
    std::unique_ptr<ast::Statement> parseValues();
    
private:
    std::vector<std::vector<std::unique_ptr<ast::Expression>>> parseValuesList();
};

} // namespace hamdb
