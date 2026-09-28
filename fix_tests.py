import sys

def fix(path):
    with open(path, "r") as f: c = f.read()
    
    # 1. Fix temp dir path
    if "test_db_ = \"test_planner.hamdb\";" in c:
        c = c.replace('test_db_ = "test_planner.hamdb";', 'test_db_ = std::filesystem::temp_directory_path() / ("test_planner_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".hamdb");')
    elif 'test_db_ = "test_physical_planner.hamdb";' in c:
        c = c.replace('test_db_ = "test_physical_planner.hamdb";', 'test_db_ = std::filesystem::temp_directory_path() / ("test_physical_planner_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".hamdb");')
    elif 'std::filesystem::unique_path' in c:
        c = c.replace('std::filesystem::unique_path("test_planner_%%%%.hamdb")', '("test_planner_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".hamdb")')
        c = c.replace('std::filesystem::unique_path("test_physical_planner_%%%%.hamdb")', '("test_physical_planner_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".hamdb")')
    
    # 2. Fix BoundSelectStatement
    old_stmt = """auto stmt = std::make_unique<binder::BoundSelectStatement>();
    stmt->table_name_ = "test_table";"""
    
    new_stmt = """auto stmt = std::make_unique<binder::BoundSelectStatement>();
    auto base = std::make_unique<binder::BoundBaseTableReference>();
    base->table_name_ = "test_table";
    stmt->table_ = std::move(base);"""
    
    c = c.replace(old_stmt, new_stmt)
    
    with open(path, "w") as f: f.write(c)
    print(f"Fixed {path}")

fix("tests/planner/planner_test.cpp")
fix("tests/planner/physical_planner_test.cpp")
