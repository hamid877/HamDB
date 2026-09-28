import sys

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

def fix_planner(c):
    return c.replace("Schema({})", "Schema(std::vector<Column>{})")

def fix_factory(c):
    if '#include "planner/nested_loop_join_plan.hpp"' not in c:
        c = c.replace('#include "planner/executor_factory.hpp"', '#include "planner/executor_factory.hpp"\n#include "executor/nested_loop_join_executor.hpp"\n#include "planner/nested_loop_join_plan.hpp"')
    # remove the ternary that uses clone
    c = c.replace("std::move(const_cast<planner::NestedLoopJoinPlan*>(join_plan)->getPredicate() ? const_cast<planner::NestedLoopJoinPlan*>(join_plan)->getPredicate()->clone() : nullptr)", "const_cast<planner::NestedLoopJoinPlan*>(join_plan)->getPredicate() ? const_cast<planner::NestedLoopJoinPlan*>(join_plan)->getPredicate()->clone() : nullptr")
    return c

rewrite("src/planner/planner.cpp", fix_planner)
rewrite("src/planner/executor_factory.cpp", fix_factory)
