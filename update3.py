import sys
import os

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

# logical_plan.hpp
def upd_logical(c):
    if "NESTED_LOOP_JOIN" not in c:
        c = c.replace("INDEX_SCAN\n};", "INDEX_SCAN,\n    NESTED_LOOP_JOIN\n};")
    return c
rewrite("include/planner/logical_plan.hpp", upd_logical)

# physical_plan.hpp
def upd_physical(c):
    if "NESTED_LOOP_JOIN" not in c:
        c = c.replace("INDEX_SCAN\n};", "INDEX_SCAN,\n    NESTED_LOOP_JOIN\n};")
    return c
rewrite("include/planner/physical_plan.hpp", upd_physical)

# write nested_loop_join_plan.hpp
join_plan_hpp = """#pragma once
#include "planner/logical_plan.hpp"
#include "planner/physical_plan.hpp"
#include "executor/expression.hpp"
#include <memory>

namespace hamdb::planner {

class LogicalNestedLoopJoinNode : public LogicalPlanNode {
public:
    LogicalNestedLoopJoinNode(Schema output_schema, std::unique_ptr<hamdb::Expression> predicate)
        : LogicalPlanNode(LogicalPlanType::NESTED_LOOP_JOIN, std::move(output_schema)),
          predicate_(std::move(predicate)) {}
          
    const hamdb::Expression* getPredicate() const { return predicate_.get(); }
    std::unique_ptr<hamdb::Expression> takePredicate() { return std::move(predicate_); }
private:
    std::unique_ptr<hamdb::Expression> predicate_;
    friend class PhysicalPlanner;
};

class NestedLoopJoinPlan : public AbstractPlanNode {
public:
    NestedLoopJoinPlan(Schema output_schema, std::unique_ptr<hamdb::Expression> predicate)
        : AbstractPlanNode(PhysicalPlanType::NESTED_LOOP_JOIN, std::move(output_schema)),
          predicate_(std::move(predicate)) {}
          
    const std::unique_ptr<hamdb::Expression>& getPredicate() const { return predicate_; }
    std::unique_ptr<hamdb::Expression>& getPredicate() { return predicate_; }
private:
    std::unique_ptr<hamdb::Expression> predicate_;
};

} // namespace hamdb::planner
"""
with open("include/planner/nested_loop_join_plan.hpp", "w") as f: f.write(join_plan_hpp)
print("Created include/planner/nested_loop_join_plan.hpp")

# write nested_loop_join_plan.cpp
join_plan_cpp = """#include "planner/nested_loop_join_plan.hpp"
// Nothing to implement yet, as it's inline in header
"""
with open("src/planner/nested_loop_join_plan.cpp", "w") as f: f.write(join_plan_cpp)
print("Created src/planner/nested_loop_join_plan.cpp")
