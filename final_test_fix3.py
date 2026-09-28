import sys
import re

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

def fix_binder(c):
    c = c.replace("stmt.table_name", "stmt.table_name_")
    c = c.replace("stmt.table_alias", "stmt.table_alias_")
    # For AST nodes that had `table_name` replaced by `table_name_`
    c = c.replace("expr.table_name_", "expr.table_name") # Wait, did ColumnValueExpression get renamed? 
    # Let's just fix it if it's there
    return c

def fix_ast(c):
    # Actually wait, ColumnValueExpression had `std::string table_name;`?
    # If fix_stmt replaced it, then expr.table_name_ is right for AST node.
    pass

def fix_physical_planner_test(c):
    bad_insert = """    auto base = std::make_unique<hamdb::binder::BoundBaseTableReference>();
    base->table_name_ = "test_table";
    stmt->table_ = std::move(base);"""
    c = c.replace(bad_insert, '    stmt->table_name_ = "test_table";')
    return c

rewrite("src/binder/binder.cpp", fix_binder)
rewrite("tests/planner/physical_planner_test.cpp", fix_physical_planner_test)
