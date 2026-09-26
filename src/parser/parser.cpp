#include "parser/parser.hpp"

namespace hamdb {

Parser::Parser(std::string_view source)
    : lexer_(source),
      current_token_(TokenType::Eof, "", 0, 0),
      previous_token_(TokenType::Eof, "", 0, 0) {
    advance();
}

void Parser::advance() {
    previous_token_ = current_token_;
    while (true) {
        current_token_ = lexer_.nextToken();
        if (current_token_.type != TokenType::Invalid) {
            break;
        }
        error(current_token_, current_token_.lexeme);
    }
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(TokenType type) const {
    if (current_token_.type == TokenType::Eof) {
        return type == TokenType::Eof;
    }
    return current_token_.type == type;
}

void Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) {
        advance();
        return;
    }
    error(current_token_, message);
}

[[noreturn]] void Parser::error(const Token& token, const std::string& message) {
    std::string err = message + " at '" + token.lexeme + "'";
    throw ParserError(err, token.line, token.column);
}

std::unique_ptr<ast::Statement> Parser::parseStatement() {
    if (match(TokenType::Select)) return parseSelect();
    if (match(TokenType::Insert)) return parseInsert();
    if (match(TokenType::Update)) return parseUpdate();
    if (match(TokenType::Delete)) return parseDelete();
    if (match(TokenType::Values)) return parseValues();
    
    error(current_token_, "Expected statement");
}

std::unique_ptr<ast::Expression> Parser::parseExpression() {
    return parseOr();
}

std::unique_ptr<ast::Expression> Parser::parseOr() {
    auto expr = parseAnd();
    while (match(TokenType::Or)) {
        auto right = parseAnd();
        expr = std::make_unique<ast::BinaryExpression>(ast::BinaryExpression::Op::Or, std::move(expr), std::move(right));
    }
    return expr;
}

std::unique_ptr<ast::Expression> Parser::parseAnd() {
    auto expr = parseComparison();
    while (match(TokenType::And)) {
        auto right = parseComparison();
        expr = std::make_unique<ast::BinaryExpression>(ast::BinaryExpression::Op::And, std::move(expr), std::move(right));
    }
    return expr;
}

std::unique_ptr<ast::Expression> Parser::parseComparison() {
    auto expr = parseTerm();
    while (true) {
        if (match(TokenType::Equals)) {
            expr = std::make_unique<ast::BinaryExpression>(ast::BinaryExpression::Op::Equals, std::move(expr), parseTerm());
        } else if (match(TokenType::NotEquals)) {
            expr = std::make_unique<ast::BinaryExpression>(ast::BinaryExpression::Op::NotEquals, std::move(expr), parseTerm());
        } else if (match(TokenType::Less)) {
            expr = std::make_unique<ast::BinaryExpression>(ast::BinaryExpression::Op::Less, std::move(expr), parseTerm());
        } else if (match(TokenType::LessEquals)) {
            expr = std::make_unique<ast::BinaryExpression>(ast::BinaryExpression::Op::LessEquals, std::move(expr), parseTerm());
        } else if (match(TokenType::Greater)) {
            expr = std::make_unique<ast::BinaryExpression>(ast::BinaryExpression::Op::Greater, std::move(expr), parseTerm());
        } else if (match(TokenType::GreaterEquals)) {
            expr = std::make_unique<ast::BinaryExpression>(ast::BinaryExpression::Op::GreaterEquals, std::move(expr), parseTerm());
        } else {
            break;
        }
    }
    return expr;
}

std::unique_ptr<ast::Expression> Parser::parseTerm() {
    auto expr = parseFactor();
    while (true) {
        if (match(TokenType::Plus)) {
            expr = std::make_unique<ast::BinaryExpression>(ast::BinaryExpression::Op::Add, std::move(expr), parseFactor());
        } else if (match(TokenType::Minus)) {
            expr = std::make_unique<ast::BinaryExpression>(ast::BinaryExpression::Op::Subtract, std::move(expr), parseFactor());
        } else {
            break;
        }
    }
    return expr;
}

std::unique_ptr<ast::Expression> Parser::parseFactor() {
    auto expr = parseUnary();
    while (true) {
        if (match(TokenType::Asterisk)) {
            expr = std::make_unique<ast::BinaryExpression>(ast::BinaryExpression::Op::Multiply, std::move(expr), parseUnary());
        } else if (match(TokenType::Slash)) {
            expr = std::make_unique<ast::BinaryExpression>(ast::BinaryExpression::Op::Divide, std::move(expr), parseUnary());
        } else if (match(TokenType::Modulo)) {
            expr = std::make_unique<ast::BinaryExpression>(ast::BinaryExpression::Op::Modulo, std::move(expr), parseUnary());
        } else {
            break;
        }
    }
    return expr;
}

std::unique_ptr<ast::Expression> Parser::parseUnary() {
    if (match(TokenType::Not)) {
        return std::make_unique<ast::UnaryExpression>(ast::UnaryExpression::Op::Not, parseUnary());
    }
    if (match(TokenType::Plus)) {
        return std::make_unique<ast::UnaryExpression>(ast::UnaryExpression::Op::Plus, parseUnary());
    }
    if (match(TokenType::Minus)) {
        return std::make_unique<ast::UnaryExpression>(ast::UnaryExpression::Op::Minus, parseUnary());
    }
    return parsePrimary();
}

std::unique_ptr<ast::Expression> Parser::parsePrimary() {
    if (match(TokenType::Integer)) {
        return std::make_unique<ast::ConstantExpression>(ast::ConstantExpression::Type::Integer, previous_token_.lexeme);
    }
    if (match(TokenType::String)) {
        return std::make_unique<ast::ConstantExpression>(ast::ConstantExpression::Type::String, previous_token_.lexeme);
    }
    if (match(TokenType::BoolLiteral)) {
        // Normalize boolean literals
        std::string val = previous_token_.lexeme;
        for (auto& c : val) c = std::tolower((unsigned char)c);
        return std::make_unique<ast::ConstantExpression>(ast::ConstantExpression::Type::Boolean, val);
    }
    if (match(TokenType::NullLiteral)) {
        return std::make_unique<ast::ConstantExpression>(ast::ConstantExpression::Type::Null, "null");
    }
    if (match(TokenType::Asterisk)) {
        return std::make_unique<ast::StarExpression>();
    }
    if (match(TokenType::Identifier)) {
        std::string name = previous_token_.lexeme;
        if (match(TokenType::Dot)) {
            consume(TokenType::Identifier, "Expected column name after '.'");
            std::string col = previous_token_.lexeme;
            return std::make_unique<ast::ColumnValueExpression>(col, name);
        }
        return std::make_unique<ast::ColumnValueExpression>(name);
    }
    if (match(TokenType::LeftParen)) {
        auto expr = parseExpression();
        consume(TokenType::RightParen, "Expected ')' after expression");
        return expr;
    }
    error(current_token_, "Expected expression");
}

std::unique_ptr<ast::Statement> Parser::parseSelect() {
    auto stmt = std::make_unique<ast::SelectStatement>();
    
    // Select list
    do {
        stmt->select_list.push_back(parseExpression());
    } while (match(TokenType::Comma));

    // From
    if (match(TokenType::From)) {
        consume(TokenType::Identifier, "Expected table name");
        stmt->table_name = previous_token_.lexeme;
        if (match(TokenType::Identifier)) {
            stmt->table_alias = previous_token_.lexeme;
        }
    }

    // Where
    if (match(TokenType::Where)) {
        stmt->where_clause = parseExpression();
    }

    // Order By
    if (match(TokenType::Order)) {
        consume(TokenType::By, "Expected 'BY' after 'ORDER'");
        do {
            auto expr = parseExpression();
            bool asc = true;
            if (match(TokenType::Desc)) {
                asc = false;
            } else {
                match(TokenType::Asc);
            }
            stmt->order_by.emplace_back(std::move(expr), asc);
        } while (match(TokenType::Comma));
    }

    // Limit
    if (match(TokenType::Limit)) {
        stmt->limit = parseExpression();
    }

    // Offset
    if (match(TokenType::Offset)) {
        stmt->offset = parseExpression();
    }

    return stmt;
}

std::unique_ptr<ast::Statement> Parser::parseInsert() {
    auto stmt = std::make_unique<ast::InsertStatement>();
    if (match(TokenType::Into)) {
        // optional INTO
    }
    consume(TokenType::Identifier, "Expected table name");
    stmt->table_name = previous_token_.lexeme;
    
    consume(TokenType::Values, "Expected VALUES clause");
    stmt->values = parseValuesList();
    return stmt;
}

std::unique_ptr<ast::Statement> Parser::parseUpdate() {
    auto stmt = std::make_unique<ast::UpdateStatement>();
    consume(TokenType::Identifier, "Expected table name");
    stmt->table_name = previous_token_.lexeme;
    
    consume(TokenType::Set, "Expected SET clause");
    do {
        consume(TokenType::Identifier, "Expected column name");
        std::string col = previous_token_.lexeme;
        consume(TokenType::Equals, "Expected '=' after column name");
        stmt->set_clauses.emplace_back(col, parseExpression());
    } while (match(TokenType::Comma));
    
    if (match(TokenType::Where)) {
        stmt->where_clause = parseExpression();
    }
    return stmt;
}

std::unique_ptr<ast::Statement> Parser::parseDelete() {
    auto stmt = std::make_unique<ast::DeleteStatement>();
    if (match(TokenType::From)) {
        // optional FROM
    }
    consume(TokenType::Identifier, "Expected table name");
    stmt->table_name = previous_token_.lexeme;
    
    if (match(TokenType::Where)) {
        stmt->where_clause = parseExpression();
    }
    return stmt;
}

std::vector<std::vector<std::unique_ptr<ast::Expression>>> Parser::parseValuesList() {
    std::vector<std::vector<std::unique_ptr<ast::Expression>>> values_list;
    do {
        consume(TokenType::LeftParen, "Expected '(' in VALUES");
        std::vector<std::unique_ptr<ast::Expression>> row;
        do {
            row.push_back(parseExpression());
        } while (match(TokenType::Comma));
        consume(TokenType::RightParen, "Expected ')' in VALUES");
        values_list.push_back(std::move(row));
    } while (match(TokenType::Comma));
    return values_list;
}

std::unique_ptr<ast::Statement> Parser::parseValues() {
    auto stmt = std::make_unique<ast::ValuesStatement>();
    stmt->values = parseValuesList();
    return stmt;
}

} // namespace hamdb
