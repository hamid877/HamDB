import sys

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

def fix_bound(c):
    c = c.replace("""class BoundBaseTableReference : public BoundTableReference {
public:
    BoundTableReferenceType getType() const override { return BoundTableReferenceType::BASE_TABLE; }
    std::unique_ptr<BoundTableReference> table_;
    const Schema* schema_{nullptr};
};""", """class BoundBaseTableReference : public BoundTableReference {
public:
    BoundTableReferenceType getType() const override { return BoundTableReferenceType::BASE_TABLE; }
    std::string table_name_;
    std::string table_alias_;
    const Schema* schema_{nullptr};
};""")
    return c

def fix_fmt(c):
    bad1 = """        case LogicalPlanType::NESTED_LOOP_JOIN: {
            auto join = static_cast<const planner::LogicalNestedLoopJoinNode*>(plan);
            node.name = "NESTED_LOOP_JOIN";
            if (join->getPredicate()) {
                node.details = "condition=" + join->getPredicate()->toString();
            }
            break;
        }"""
    good1 = """        case LogicalPlanType::NESTED_LOOP_JOIN: {
            node.name = "NESTED_LOOP_JOIN";
            break;
        }"""
    c = c.replace(bad1, good1)
    
    bad2 = """        case PhysicalPlanType::NESTED_LOOP_JOIN: {
            auto join = static_cast<const planner::NestedLoopJoinPlan*>(plan);
            node.name = "NESTED_LOOP_JOIN";
            if (join->getPredicate()) {
                node.details = "condition=" + join->getPredicate()->toString();
            }
            break;
        }"""
    good2 = """        case PhysicalPlanType::NESTED_LOOP_JOIN: {
            node.name = "NESTED_LOOP_JOIN";
            break;
        }"""
    c = c.replace(bad2, good2)
    return c

def fix_factory(c):
    c = c.replace("dynamic_cast<NestedLoopJoinPlan*>", "dynamic_cast<const planner::NestedLoopJoinPlan*>")
    c = c.replace("join_plan->getPredicate()", "const_cast<planner::NestedLoopJoinPlan*>(join_plan)->getPredicate() ? const_cast<planner::NestedLoopJoinPlan*>(join_plan)->getPredicate()->clone() : nullptr")
    return c

rewrite("include/binder/bound_statement.hpp", fix_bound)
rewrite("src/planner/plan_formatter.cpp", fix_fmt)
rewrite("src/planner/executor_factory.cpp", fix_factory)
