import sys

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

def fix_planner_test(c):
    # Fix the std::filesystem::path addition
    c = c.replace('std::filesystem::temp_directory_path() / "test_planner_" + std::to_string', 
                  'std::filesystem::temp_directory_path() / ("test_planner_" + std::to_string')
    c = c.replace('.count()) + ".hamdb";', '.count()) + ".hamdb");')
    
    # Fix BoundInsertStatement in PlannerTest_PlanInsertValues_Test
    bad_insert = """    auto base = std::make_unique<hamdb::binder::BoundBaseTableReference>();
    base->table_name_ = "test_table";
    stmt->table_ = std::move(base);"""
    
    if "PlannerTest_PlanInsertValues_Test" in c:
        parts = c.split("TEST_F(PlannerTest, PlanInsertValues)")
        parts[1] = parts[1].replace(bad_insert, '    stmt->table_name_ = "test_table";')
        c = "TEST_F(PlannerTest, PlanInsertValues)".join(parts)
        
    if "PlannerTest_PlanUpdate_Test" in c:
        parts = c.split("TEST_F(PlannerTest, PlanUpdate)")
        parts[1] = parts[1].replace(bad_insert, '    stmt->table_name_ = "test_table";')
        c = "TEST_F(PlannerTest, PlanUpdate)".join(parts)
        
    if "PlannerTest_PlanDelete_Test" in c:
        parts = c.split("TEST_F(PlannerTest, PlanDelete)")
        parts[1] = parts[1].replace(bad_insert, '    stmt->table_name_ = "test_table";')
        c = "TEST_F(PlannerTest, PlanDelete)".join(parts)
        
    return c

def fix_physical_planner_test(c):
    c = c.replace('std::filesystem::temp_directory_path() / "test_planner_" + std::to_string', 
                  'std::filesystem::temp_directory_path() / ("test_planner_" + std::to_string')
    c = c.replace('.count()) + ".hamdb";', '.count()) + ".hamdb");')
    return c

rewrite("tests/planner/planner_test.cpp", fix_planner_test)
rewrite("tests/planner/physical_planner_test.cpp", fix_physical_planner_test)
