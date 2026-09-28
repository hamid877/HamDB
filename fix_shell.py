import sys

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

def fix_shell(c):
    if '#include "planner/nested_loop_join_plan.hpp"' not in c:
        c = c.replace('#include "planner/physical_plan.hpp"', '#include "planner/physical_plan.hpp"\n#include "planner/nested_loop_join_plan.hpp"')
    
    join_clone = """        case planner::PhysicalPlanType::NESTED_LOOP_JOIN: {
            auto* node = static_cast<const planner::NestedLoopJoinPlan*>(plan);
            cloned = std::make_unique<planner::NestedLoopJoinPlan>(
                node->getOutputSchema(),
                node->getPredicate() ? node->getPredicate()->clone() : nullptr);
            break;
        }"""
    
    if "PhysicalPlanType::NESTED_LOOP_JOIN" not in c:
        c = c.replace("case planner::PhysicalPlanType::INDEX_SCAN:", join_clone + "\n        case planner::PhysicalPlanType::INDEX_SCAN:")
    
    join_bind = """        case planner::PhysicalPlanType::NESTED_LOOP_JOIN: {
            auto* node = static_cast<planner::NestedLoopJoinPlan*>(plan);
            if (node->getPredicate()) node->getPredicate()->bindParameters(params);
            break;
        }"""
    
    if "PhysicalPlanType::NESTED_LOOP_JOIN:" not in c.split("bindPhysicalPlan")[1]:
        c = c.replace("case planner::PhysicalPlanType::INDEX_SCAN: {", join_bind + "\n        case planner::PhysicalPlanType::INDEX_SCAN: {")
    return c

rewrite("src/shell/shell.cpp", fix_shell)
