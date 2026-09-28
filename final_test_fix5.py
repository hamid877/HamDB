import sys

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

def fix_join_test(c):
    c = c.replace('std::filesystem::temp_directory_path() / "test_join_plan_"', 'std::filesystem::temp_directory_path() / ("test_join_plan_"')
    c = c.replace('.hamdb";', '.hamdb");')
    return c

rewrite("tests/planner/join_plan_test.cpp", fix_join_test)
