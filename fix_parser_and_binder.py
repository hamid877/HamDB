import sys
import re

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

def fix_parser(c):
    # Remove extra }
    c = c.replace("""    // From
    if (match(TokenType::From)) {
        stmt->table = parseTableReference();
    }
    }

    // Where""", """    // From
    if (match(TokenType::From)) {
        stmt->table = parseTableReference();
    }

    // Where""")
    
    # Fix DeleteStatement
    c = c.replace("""std::unique_ptr<ast::Statement> Parser::parseDelete() {
    auto stmt = std::make_unique<ast::DeleteStatement>();
    if (match(TokenType::From)) {
        stmt->table = parseTableReference();
    }
    consume(TokenType::Identifier, "Expected table name");
    stmt->table_name = previous_token_.lexeme;
    
    if (match(TokenType::Where)) {
        stmt->where_clause = parseExpression();
    }
    return stmt;
}""", """std::unique_ptr<ast::Statement> Parser::parseDelete() {
    auto stmt = std::make_unique<ast::DeleteStatement>();
    if (match(TokenType::From)) {
        // optional FROM
    }
    consume(TokenType::Identifier, "Expected table name");
    stmt->table_name_ = previous_token_.lexeme;
    
    if (match(TokenType::Where)) {
        stmt->where_clause = parseExpression();
    }
    return stmt;
}""")

    # Fix table_name properties which are now table_name_
    c = c.replace("stmt->table_name = previous_token_.lexeme;", "stmt->table_name_ = previous_token_.lexeme;")
    c = c.replace("base->table_name = previous_token_.lexeme;", "base->table_name_ = previous_token_.lexeme;")
    c = c.replace("right_base->table_name = previous_token_.lexeme;", "right_base->table_name_ = previous_token_.lexeme;")
    
    c = c.replace("base->table_alias = previous_token_.lexeme;", "base->table_alias_ = previous_token_.lexeme;")
    c = c.replace("right_base->table_alias = previous_token_.lexeme;", "right_base->table_alias_ = previous_token_.lexeme;")
    
    c = c.replace("join->left = std::move(current);", "join->left_ = std::move(current);")
    c = c.replace("join->right = std::move(right_base);", "join->right_ = std::move(right_base);")
    c = c.replace("join->condition = parseExpression();", "join->condition_ = parseExpression();")
    
    return c

def fix_binder(c):
    c = c.replace("!expr.table_name_.empty()", "!expr.table_name.empty()")
    c = c.replace("expr.table_name_", "expr.table_name")
    return c

rewrite("src/parser/parser.cpp", fix_parser)
rewrite("src/binder/binder.cpp", fix_binder)
