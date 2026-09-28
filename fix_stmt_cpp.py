import sys
import re

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

def fix_stmt_cpp(c):
    c = c.replace("table_name", "table_name_")
    c = c.replace("table_alias", "table_alias_")
    c = c.replace("left->", "left_->")
    c = c.replace("right->", "right_->")
    c = c.replace("condition->", "condition_->")
    c = c.replace("if (condition)", "if (condition_)")
    # Wait, the replace string "table_name_" might have matched "table_name_" and made it "table_name__" if already underscore.
    c = c.replace("table_name__", "table_name_")
    c = c.replace("table_alias__", "table_alias_")
    c = c.replace("left__", "left_")
    c = c.replace("right__", "right_")
    c = c.replace("condition__", "condition_")
    return c

rewrite("src/parser/ast/statement.cpp", fix_stmt_cpp)
