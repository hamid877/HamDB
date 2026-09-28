import sys
import re

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

# binder.hpp
binder_hpp_ctx = """
    struct TableContext {
        std::string table_name;
        std::string table_alias;
        const Schema* schema;
        size_t column_offset;
    };
    struct Context {
        std::vector<TableContext> tables;
        std::unique_ptr<Schema> combined_schema;
    };
"""
def upd_binder_hpp(c):
    if "std::unique_ptr<BoundTableReference> bindTableReference" not in c:
        c = re.sub(r'struct Context \{[\s\S]*?\};', binder_hpp_ctx, c, count=1)
        c = c.replace("std::unique_ptr<BoundSelectStatement> bindSelect(const ast::SelectStatement& stmt);", "std::unique_ptr<BoundTableReference> bindTableReference(const ast::TableReference& ref, std::vector<Column>& combined_columns);\n    std::unique_ptr<BoundSelectStatement> bindSelect(const ast::SelectStatement& stmt);")
    return c
rewrite("include/binder/binder.hpp", upd_binder_hpp)
