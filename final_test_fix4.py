import sys
import re

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

def fix_planner_test(c):
    # Fix the std::filesystem::path addition
    c = c.replace('std::filesystem::unique_path("test_planner_%%%%.hamdb")', '"test_planner_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".hamdb"')
    c = c.replace('std::filesystem::temp_directory_path() / "test_planner_"', 'std::filesystem::temp_directory_path() / ("test_planner_"')
    
    # We only want to replace stmt->table_name_ in Select statements!
    bad_select = """    auto stmt = std::make_unique<hamdb::binder::BoundSelectStatement>();
    stmt->table_name_ = "test_table";"""
    good_select = """    auto stmt = std::make_unique<hamdb::binder::BoundSelectStatement>();
    auto base = std::make_unique<hamdb::binder::BoundBaseTableReference>();
    base->table_name_ = "test_table";
    stmt->table_ = std::move(base);"""
    c = c.replace(bad_select, good_select)
    return c

def fix_join_test(c):
    c = c.replace('std::filesystem::unique_path("test_join_plan_%%%%.hamdb")', '"test_join_plan_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".hamdb"')
    c = c.replace('catalog_->createTable("t1", schema_t1, info1);', '(void)catalog_->createTable("t1", schema_t1, info1);')
    c = c.replace('catalog_->createTable("t2", schema_t2, info2);', '(void)catalog_->createTable("t2", schema_t2, info2);')
    return c

rewrite("tests/planner/planner_test.cpp", fix_planner_test)
rewrite("tests/planner/physical_planner_test.cpp", fix_planner_test)
rewrite("tests/planner/join_plan_test.cpp", fix_join_test)
