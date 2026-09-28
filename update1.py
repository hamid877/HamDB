import sys
import re
import os

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

# token.hpp
def upd_token_hpp(c):
    return c.replace("Table, Explain, Analyze,", "Table, Explain, Analyze, Join, Inner, On,")
rewrite("include/parser/token.hpp", upd_token_hpp)

# token.cpp
def upd_token_cpp(c):
    return c.replace('case TokenType::Table: return "Table";',
                     'case TokenType::Table: return "Table";\n        case TokenType::Join: return "Join";\n        case TokenType::Inner: return "Inner";\n        case TokenType::On: return "On";')
rewrite("src/parser/token.cpp", upd_token_cpp)

# lexer.cpp
def upd_lexer_cpp(c):
    return c.replace('{"TABLE", TokenType::Table},',
                     '{"TABLE", TokenType::Table},\n        {"JOIN", TokenType::Join},\n        {"INNER", TokenType::Inner},\n        {"ON", TokenType::On},')
rewrite("src/parser/lexer.cpp", upd_lexer_cpp)

# ast/statement.hpp
ast_table_ref = """
class TableReference : public ASTNode {
public:
    virtual ~TableReference() = default;
    virtual std::string toString() const = 0;
};

class BaseTableReference : public TableReference {
public:
    std::string table_name;
    std::string table_alias;
    std::string toString() const override;
};

class JoinTableReference : public TableReference {
public:
    std::unique_ptr<TableReference> left;
    std::unique_ptr<TableReference> right;
    std::unique_ptr<Expression> condition;
    std::string toString() const override;
};
"""
def upd_ast_hpp(c):
    if "class TableReference" not in c:
        c = c.replace("class Statement : public ASTNode {", ast_table_ref + "\nclass Statement : public ASTNode {")
        c = c.replace("    std::string table_name;\n    std::string table_alias;", "    std::unique_ptr<TableReference> table;")
    return c
rewrite("include/parser/ast/statement.hpp", upd_ast_hpp)

# ast/statement.cpp
ast_stmt_cpp = """
std::string BaseTableReference::toString() const {
    if (table_alias.empty()) return table_name;
    return table_name + " AS " + table_alias;
}

std::string JoinTableReference::toString() const {
    std::ostringstream oss;
    oss << left->toString() << " JOIN " << right->toString();
    if (condition) oss << " ON " << condition->toString();
    return oss.str();
}
"""
def upd_ast_cpp(c):
    if "BaseTableReference::toString" not in c:
        c = c.replace("std::string SelectStatement::toString() const {", ast_stmt_cpp + "\nstd::string SelectStatement::toString() const {")
        c = c.replace('if (!table_name.empty()) {\n        oss << " FROM " << table_name;\n    }', 'if (table) {\n        oss << " FROM " << table->toString();\n    }')
    return c
rewrite("src/parser/ast/statement.cpp", upd_ast_cpp)

# parser.hpp
def upd_parser_hpp(c):
    if "parseTableReference" not in c:
        c = c.replace("std::unique_ptr<ast::Statement> parseSelect();", "std::unique_ptr<ast::Statement> parseSelect();\n    std::unique_ptr<ast::TableReference> parseTableReference();")
    return c
rewrite("include/parser/parser.hpp", upd_parser_hpp)

# parser.cpp
parser_table_ref = """
std::unique_ptr<ast::TableReference> Parser::parseTableReference() {
    auto base = std::make_unique<ast::BaseTableReference>();
    consume(TokenType::Identifier, "Expected table name");
    base->table_name = previous_token_.lexeme;
    if (match(TokenType::As)) {
        consume(TokenType::Identifier, "Expected table alias");
        base->table_alias = previous_token_.lexeme;
    } else if (match(TokenType::Identifier)) {
        base->table_alias = previous_token_.lexeme;
    }
    
    std::unique_ptr<ast::TableReference> current = std::move(base);
    while (match(TokenType::Inner) || match(TokenType::Join)) {
        if (previous_token_.type == TokenType::Inner) {
            consume(TokenType::Join, "Expected JOIN after INNER");
        }
        auto join = std::make_unique<ast::JoinTableReference>();
        join->left = std::move(current);
        
        auto right_base = std::make_unique<ast::BaseTableReference>();
        consume(TokenType::Identifier, "Expected table name in JOIN");
        right_base->table_name = previous_token_.lexeme;
        if (match(TokenType::As)) {
            consume(TokenType::Identifier, "Expected table alias");
            right_base->table_alias = previous_token_.lexeme;
        } else if (match(TokenType::Identifier)) {
            right_base->table_alias = previous_token_.lexeme;
        }
        join->right = std::move(right_base);
        
        if (match(TokenType::On)) {
            join->condition = parseExpression();
        }
        current = std::move(join);
    }
    return current;
}
"""
def upd_parser_cpp(c):
    if "Parser::parseTableReference" not in c:
        c = c.replace("std::unique_ptr<ast::Statement> Parser::parseSelect() {", parser_table_ref + "\nstd::unique_ptr<ast::Statement> Parser::parseSelect() {")
        c = re.sub(r'if \(match\(TokenType::From\)\) \{[\s\S]*?\}', 'if (match(TokenType::From)) {\n        stmt->table = parseTableReference();\n    }', c)
    return c
rewrite("src/parser/parser.cpp", upd_parser_cpp)

# bound_statement.hpp
bound_table_ref = """
enum class BoundTableReferenceType {
    BASE_TABLE,
    JOIN
};

class BoundTableReference {
public:
    virtual ~BoundTableReference() = default;
    virtual BoundTableReferenceType getType() const = 0;
};

class BoundBaseTableReference : public BoundTableReference {
public:
    BoundTableReferenceType getType() const override { return BoundTableReferenceType::BASE_TABLE; }
    std::string table_name_;
    std::string table_alias_;
    const Schema* schema_{nullptr};
};

class BoundJoinTable : public BoundTableReference {
public:
    BoundTableReferenceType getType() const override { return BoundTableReferenceType::JOIN; }
    std::unique_ptr<BoundTableReference> left_;
    std::unique_ptr<BoundTableReference> right_;
    std::unique_ptr<BoundExpression> condition_;
};
"""
def upd_bound_stmt_hpp(c):
    if "BoundTableReference" not in c:
        c = c.replace("class BoundStatement {", bound_table_ref + "\nclass BoundStatement {")
        c = c.replace("std::string table_name_;\n    std::string table_alias_;", "std::unique_ptr<BoundTableReference> table_;")
    return c
rewrite("include/binder/bound_statement.hpp", upd_bound_stmt_hpp)
