import sys

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

def fix_fmt(c):
    bad1 = """        case LogicalPlanType::NESTED_LOOP_JOIN: {
            auto join = static_cast<const planner::LogicalNestedLoopJoinNode*>(node);
            out += "NestedLoopJoin: {";
            if (join->getPredicate()) out += "condition=" + join->getPredicate()->toString();
            out += "}";
            break;
        }"""
    good1 = """        case LogicalPlanType::NESTED_LOOP_JOIN: {
            auto join = static_cast<const planner::LogicalNestedLoopJoinNode*>(plan);
            node.name = "NESTED_LOOP_JOIN";
            if (join->getPredicate()) {
                node.details = "condition=" + join->getPredicate()->toString();
            }
            break;
        }"""
    c = c.replace(bad1, good1)
    
    bad2 = """        case PhysicalPlanType::NESTED_LOOP_JOIN: {
            auto join = static_cast<const planner::NestedLoopJoinPlan*>(node);
            out += "NestedLoopJoin: {";
            if (join->getPredicate()) out += "condition=" + join->getPredicate()->toString();
            out += "}";
            break;
        }"""
    good2 = """        case PhysicalPlanType::NESTED_LOOP_JOIN: {
            auto join = static_cast<const planner::NestedLoopJoinPlan*>(plan);
            node.name = "NESTED_LOOP_JOIN";
            if (join->getPredicate()) {
                node.details = "condition=" + join->getPredicate()->toString();
            }
            break;
        }"""
    c = c.replace(bad2, good2)
    return c

rewrite("src/planner/plan_formatter.cpp", fix_fmt)
