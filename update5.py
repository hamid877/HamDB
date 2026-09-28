import sys
import re

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

# executor_factory.cpp
def upd_exec_factory(c):
    if "PhysicalPlanType::NESTED_LOOP_JOIN:" not in c:
        c = c.replace('#include "executor/executor_factory.hpp"', '#include "executor/executor_factory.hpp"\n#include "executor/nested_loop_join_executor.hpp"\n#include "planner/nested_loop_join_plan.hpp"')
        join_case = """
        case PhysicalPlanType::NESTED_LOOP_JOIN: {
            auto* join_plan = dynamic_cast<NestedLoopJoinPlan*>(plan.get());
            exec = std::make_unique<NestedLoopJoinExecutor>(
                std::move(child_executors[0]), std::move(child_executors[1]), std::move(join_plan->getPredicate()));
            break;
        }"""
        c = c.replace('default:\n            throw std::runtime_error("Unsupported physical plan type");', join_case + '\n        default:\n            throw std::runtime_error("Unsupported physical plan type");')
    return c
rewrite("src/planner/executor_factory.cpp", upd_exec_factory)

