import sys

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

def fix(c):
    return c.replace('catalog_->getTable("test_table", info);', '(void)catalog_->getTable("test_table", info);')

rewrite("tests/planner/planner_test.cpp", fix)
rewrite("tests/planner/physical_planner_test.cpp", fix)
