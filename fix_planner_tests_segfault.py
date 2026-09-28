import sys
import re

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

def fix_planner_tests(c):
    old_stmt = """auto stmt = std::make_unique<binder::BoundSelectStatement>();
    auto base = std::make_unique<binder::BoundBaseTableReference>();
    base->table_name_ = "test_table";
    stmt->table_ = std::move(base);"""
    
    new_stmt = """auto stmt = std::make_unique<binder::BoundSelectStatement>();
    auto base = std::make_unique<binder::BoundBaseTableReference>();
    base->table_name_ = "test_table";
    TableInfo* info;
    catalog_->getTable("test_table", info);
    base->schema_ = &info->getSchema();
    stmt->table_ = std::move(base);"""
    
    c = c.replace(old_stmt, new_stmt)
    return c

rewrite("tests/planner/planner_test.cpp", fix_planner_tests)
rewrite("tests/planner/physical_planner_test.cpp", fix_planner_tests)
