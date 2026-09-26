#include "parser/lexer.hpp"
#include <cctype>
#include <unordered_map>
#include <algorithm>

namespace hamdb {

Lexer::Lexer(std::string_view source) : source_(source) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        Token token = nextToken();
        tokens.push_back(token);
        if (token.type == TokenType::Eof) {
            break;
        }
    }
    return tokens;
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source_[current_];
}

char Lexer::peekNext() const {
    if (current_ + 1 >= source_.length()) return '\0';
    return source_[current_ + 1];
}

char Lexer::advance() {
    char c = source_[current_];
    current_++;
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

bool Lexer::isAtEnd() const {
    return current_ >= source_.length();
}

void Lexer::skipWhitespace() {
    while (true) {
        char c = peek();
        if (c == ' ' || c == '\r' || c == '\t' || c == '\n') {
            advance();
        } else if (c == '-' && peekNext() == '-') {
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
        } else if (c == '/' && peekNext() == '*') {
            advance(); // '/'
            advance(); // '*'
            while (!isAtEnd()) {
                if (peek() == '*' && peekNext() == '/') {
                    advance(); // '*'
                    advance(); // '/'
                    break;
                }
                advance();
            }
        } else {
            break;
        }
    }
}

Token Lexer::makeToken(TokenType type) {
    std::string_view lexeme = source_.substr(start_, current_ - start_);
    return Token(type, std::string(lexeme), line_, start_column_);
}

Token Lexer::makeToken(TokenType type, std::string lexeme) {
    return Token(type, std::move(lexeme), line_, start_column_);
}

Token Lexer::makeErrorToken(std::string message) {
    return Token(TokenType::Invalid, std::move(message), line_, start_column_);
}

Token Lexer::number() {
    while (!isAtEnd() && std::isdigit(peek())) {
        advance();
    }
    return makeToken(TokenType::Integer);
}

Token Lexer::string() {
    std::string value;
    // We already advanced past the opening quote
    while (!isAtEnd()) {
        if (peek() == '\'') {
            if (peekNext() == '\'') {
                // Escaped quote
                value.push_back('\'');
                advance();
                advance();
            } else {
                // End of string
                advance(); // consume closing quote
                return makeToken(TokenType::String, value);
            }
        } else {
            value.push_back(advance());
        }
    }
    return makeErrorToken("Unterminated string");
}

Token Lexer::identifierOrKeyword() {
    while (!isAtEnd() && (std::isalnum(peek()) || peek() == '_')) {
        advance();
    }
    
    std::string_view lexeme = source_.substr(start_, current_ - start_);
    std::string upper_lexeme;
    upper_lexeme.reserve(lexeme.size());
    for (char c : lexeme) {
        upper_lexeme.push_back(std::toupper(static_cast<unsigned char>(c)));
    }

    static const std::unordered_map<std::string, TokenType> keywords = {
        {"SELECT", TokenType::Select},
        {"INSERT", TokenType::Insert},
        {"UPDATE", TokenType::Update},
        {"DELETE", TokenType::Delete},
        {"FROM", TokenType::From},
        {"WHERE", TokenType::Where},
        {"AND", TokenType::And},
        {"OR", TokenType::Or},
        {"NOT", TokenType::Not},
        {"LIMIT", TokenType::Limit},
        {"OFFSET", TokenType::Offset},
        {"ORDER", TokenType::Order},
        {"BY", TokenType::By},
        {"ASC", TokenType::Asc},
        {"DESC", TokenType::Desc},
        {"VALUES", TokenType::Values},
        {"INTO", TokenType::Into},
        {"SET", TokenType::Set},
        {"CREATE", TokenType::Create},
        {"TABLE", TokenType::Table},
        {"INT", TokenType::Int},
        {"BOOLEAN", TokenType::Boolean},
        {"VARCHAR", TokenType::Varchar},
        {"PRIMARY", TokenType::Primary},
        {"KEY", TokenType::Key},
        {"NULL", TokenType::NullLiteral},
        {"TRUE", TokenType::BoolLiteral},
        {"FALSE", TokenType::BoolLiteral}
    };

    auto it = keywords.find(upper_lexeme);
    if (it != keywords.end()) {
        return makeToken(it->second);
    }
    return makeToken(TokenType::Identifier);
}

Token Lexer::nextToken() {
    skipWhitespace();

    start_ = current_;
    
    // Calculate the start line and column for the new token based on current line/col.
    // If skipWhitespace consumed newlines, line_ and column_ are already updated.
    start_column_ = column_;
    std::size_t start_line = line_;

    if (isAtEnd()) return Token(TokenType::Eof, "", start_line, start_column_);

    char c = advance();

    if (std::isalpha(c) || c == '_') {
        Token t = identifierOrKeyword();
        t.line = start_line; // ensure token line is start_line
        return t;
    }
    if (std::isdigit(c)) {
        Token t = number();
        t.line = start_line;
        return t;
    }

    Token t(TokenType::Invalid, "", start_line, start_column_);
    switch (c) {
        case '(': t = makeToken(TokenType::LeftParen); break;
        case ')': t = makeToken(TokenType::RightParen); break;
        case ',': t = makeToken(TokenType::Comma); break;
        case ';': t = makeToken(TokenType::Semicolon); break;
        case '.': t = makeToken(TokenType::Dot); break;
        case '+': t = makeToken(TokenType::Plus); break;
        case '-': t = makeToken(TokenType::Minus); break;
        case '*': t = makeToken(TokenType::Asterisk); break;
        case '/': t = makeToken(TokenType::Slash); break;
        case '%': t = makeToken(TokenType::Modulo); break;
        case '=': t = makeToken(TokenType::Equals); break;
        case '!':
            if (peek() == '=') {
                advance();
                t = makeToken(TokenType::NotEquals);
            } else {
                t = makeErrorToken("Invalid character");
            }
            break;
        case '<':
            if (peek() == '=') {
                advance();
                t = makeToken(TokenType::LessEquals);
            } else {
                t = makeToken(TokenType::Less);
            }
            break;
        case '>':
            if (peek() == '=') {
                advance();
                t = makeToken(TokenType::GreaterEquals);
            } else {
                t = makeToken(TokenType::Greater);
            }
            break;
        case '\'': 
            t = string(); 
            break;
        default:
            t = makeErrorToken(std::string("Invalid character: ") + c);
            break;
    }
    t.line = start_line;
    return t;
}

} // namespace hamdb
