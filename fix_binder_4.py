import sys
import re

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

# binder.hpp
def upd_binder_hpp(c):
    ctx_rep = """struct TableContext {
        std::string table_name;
        std::string table_alias;
        const Schema* schema;
        size_t column_offset;
    };
    struct Context {
        std::vector<TableContext> tables;
        std::unique_ptr<Schema> combined_schema;
    };"""
    c = re.sub(r'struct Context \{[\s\S]*?\};', ctx_rep, c)
    return c

# shell.cpp
def upd_shell_cpp(c):
    if '#include "planner/nested_loop_join_plan.hpp"' not in c:
        c = c.replace('#include "planner/plan_formatter.hpp"', '#include "planner/plan_formatter.hpp"\n#include "planner/nested_loop_join_plan.hpp"')
    return c

rewrite("include/binder/binder.hpp", upd_binder_hpp)
rewrite("src/shell/shell.cpp", upd_shell_cpp)
