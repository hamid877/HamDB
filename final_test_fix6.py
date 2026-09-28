import sys
import re

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

def fix_binder(c):
    # Only replace exactly what needs to be replaced
    
    # 1. BaseTableReference properties (because I renamed them in AST)
    c = c.replace("base->table_name", "base->table_name_")
    c = c.replace("base->table_alias", "base->table_alias_")
    
    # Wait, the replace string "base->table_name" is contained in "base->table_name_"!
    # So if it was already "base->table_name_", it becomes "base->table_name__"
    c = c.replace("base->table_name__", "base->table_name_")
    c = c.replace("base->table_alias__", "base->table_alias_")
    
    # 2. Join properties
    c = c.replace("join->left", "join->left_")
    c = c.replace("join->right", "join->right_")
    c = c.replace("join->condition", "join->condition_")
    
    c = c.replace("join->left__", "join->left_")
    c = c.replace("join->right__", "join->right_")
    c = c.replace("join->condition__", "join->condition_")
    
    # 3. stmt properties (Insert, Update, Delete)
    # The error was: ‘const class hamdb::ast::UpdateStatement’ has no member named ‘table_name’; did you mean ‘table_name_’?
    # It happens in: current_context_.tables.push_back({stmt.table_name, "", &table_info->getSchema(), 0});
    c = c.replace("stmt.table_name", "stmt.table_name_")
    c = c.replace("stmt.table_name__", "stmt.table_name_")
    c = c.replace("stmt.table_alias", "stmt.table_alias_")
    c = c.replace("stmt.table_alias__", "stmt.table_alias_")
    return c

def fix_planner_tests(c):
    # Fix the std::filesystem::path addition
    c = c.replace('std::filesystem::unique_path("test_planner_%%%%.hamdb")', '"test_planner_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".hamdb"')
    c = c.replace('std::filesystem::temp_directory_path() / "test_planner_"', 'std::filesystem::temp_directory_path() / ("test_planner_"')
    c = c.replace('.hamdb";', '.hamdb");')
    
    # ONLY replace in SelectStatements
    c = re.sub(
        r'auto stmt = std::make_unique<hamdb::binder::BoundSelectStatement>\(\);\s+stmt->table_name_ = "test_table";',
        'auto stmt = std::make_unique<hamdb::binder::BoundSelectStatement>();\n    auto base = std::make_unique<hamdb::binder::BoundBaseTableReference>();\n    base->table_name_ = "test_table";\n    stmt->table_ = std::move(base);',
        c
    )
    # And specifically for PlannerTest_PlanSelectWithFilterAndLimit_Test which has comments in between
    c = re.sub(
        r'stmt->table_name_ = "test_table";',
        'auto base = std::make_unique<hamdb::binder::BoundBaseTableReference>();\n    base->table_name_ = "test_table";\n    stmt->table_ = std::move(base);',
        c
    )
    # Revert it for Insert, Update, Delete tests which have different statements
    c = c.replace(
        'auto base = std::make_unique<hamdb::binder::BoundBaseTableReference>();\n    base->table_name_ = "test_table";\n    stmt->table_ = std::move(base);',
        'stmt->table_name_ = "test_table";'
    )
    # Apply it again specifically for SelectStatement tests only!
    # A cleaner way is just to manually write replace
    return c

# The cleaner way for planner tests:
def fix_planner_tests_clean(c):
    c = c.replace('std::filesystem::unique_path("test_planner_%%%%.hamdb")', '"test_planner_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".hamdb"')
    c = c.replace('std::filesystem::temp_directory_path() / "test_planner_"', 'std::filesystem::temp_directory_path() / ("test_planner_"')
    c = c.replace('.hamdb";', '.hamdb");')
    
    # We will search for BoundSelectStatement and the next `table_name_ = "test_table"`
    parts = c.split("BoundSelectStatement")
    for i in range(1, len(parts)):
        parts[i] = parts[i].replace('stmt->table_name_ = "test_table";', 'auto base = std::make_unique<hamdb::binder::BoundBaseTableReference>();\n    base->table_name_ = "test_table";\n    stmt->table_ = std::move(base);', 1)
    
    return "BoundSelectStatement".join(parts)

rewrite("src/binder/binder.cpp", fix_binder)
rewrite("tests/planner/planner_test.cpp", fix_planner_tests_clean)
rewrite("tests/planner/physical_planner_test.cpp", fix_planner_tests_clean)
