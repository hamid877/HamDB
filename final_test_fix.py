import sys
import re

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

def fix_binder(c):
    c = c.replace("base->table_name", "base->table_name_")
    c = c.replace("base->table_alias", "base->table_alias_")
    c = c.replace("join->left", "join->left_")
    c = c.replace("join->right", "join->right_")
    c = c.replace("join->condition", "join->condition_")
    return c

def fix_stmt(c):
    # Just in case AST base table ref doesn't have _
    if "std::string table_name;" in c:
        c = c.replace("std::string table_name;", "std::string table_name_;")
        c = c.replace("std::string table_alias;", "std::string table_alias_;")
        c = c.replace("std::unique_ptr<TableReference> left;", "std::unique_ptr<TableReference> left_;")
        c = c.replace("std::unique_ptr<TableReference> right;", "std::unique_ptr<TableReference> right_;")
        c = c.replace("std::unique_ptr<Expression> condition;", "std::unique_ptr<Expression> condition_;")
    return c

def fix_binder_test(c):
    c = c.replace('ASSERT_EQ(sel->table_name_, "users");', 
                  'auto base = dynamic_cast<hamdb::binder::BoundBaseTableReference*>(sel->table_.get());\n    ASSERT_TRUE(base != nullptr);\n    ASSERT_EQ(base->table_name_, "users");')
    c = c.replace('ASSERT_EQ(sel->table_alias_, "u");', 
                  'ASSERT_EQ(base->table_alias_, "u");')
    return c

def fix_planner_test(c):
    c = c.replace('std::filesystem::unique_path("test_planner_%%%%.hamdb")', '"test_planner_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".hamdb"')
    c = c.replace('stmt->table_name_ = "test_table";', 
                  'auto base = std::make_unique<hamdb::binder::BoundBaseTableReference>();\n    base->table_name_ = "test_table";\n    stmt->table_ = std::move(base);')
    return c

def fix_const_fold(c):
    return c.replace("case planner::LogicalPlanType::INDEX_SCAN:", "case planner::LogicalPlanType::NESTED_LOOP_JOIN:\n        case planner::LogicalPlanType::INDEX_SCAN:")

rewrite("src/binder/binder.cpp", fix_binder)
rewrite("include/parser/ast/statement.hpp", fix_stmt)
rewrite("tests/binder/binder_test.cpp", fix_binder_test)
rewrite("tests/planner/planner_test.cpp", fix_planner_test)
rewrite("tests/planner/physical_planner_test.cpp", fix_planner_test)
rewrite("src/optimizer/constant_folding_rule.cpp", fix_const_fold)
