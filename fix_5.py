import sys
import re

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

def fix_binder(c):
    return c.replace("    };\n    };\n    Context current_context_;", "    };\n    Context current_context_;")

def fix_shell(c):
    if '#include "planner/nested_loop_join_plan.hpp"' not in c:
        c = c.replace('#include "planner/plan_formatter.hpp"', '#include "planner/plan_formatter.hpp"\n#include "planner/nested_loop_join_plan.hpp"')
    
    join_clone = """        case planner::PhysicalPlanType::NESTED_LOOP_JOIN: {
            auto* node = static_cast<const planner::NestedLoopJoinPlan*>(plan);
            cloned = std::make_unique<planner::NestedLoopJoinPlan>(
                node->getOutputSchema(),
                node->getPredicate() ? node->getPredicate()->clone() : nullptr);
            break;
        }"""
    
    c = c.replace("case planner::PhysicalPlanType::INDEX_SCAN: {", join_clone + "\n        case planner::PhysicalPlanType::INDEX_SCAN: {", 1)
    
    join_bind = """        case planner::PhysicalPlanType::NESTED_LOOP_JOIN: {
            auto* node = static_cast<planner::NestedLoopJoinPlan*>(plan);
            if (node->getPredicate()) node->getPredicate()->bindParameters(params);
            break;
        }"""
    
    parts = c.split("void bindPhysicalPlan")
    parts[1] = parts[1].replace("case planner::PhysicalPlanType::INDEX_SCAN: {", join_bind + "\n        case planner::PhysicalPlanType::INDEX_SCAN: {")
    return "void bindPhysicalPlan".join(parts)

rewrite("include/binder/binder.hpp", fix_binder)
rewrite("src/shell/shell.cpp", fix_shell)
