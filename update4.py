import sys
import re

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

# planner.hpp
def upd_planner_hpp(c):
    if "planTableReference" not in c:
        c = c.replace("std::unique_ptr<LogicalPlanNode> planSelect(binder::BoundSelectStatement* stmt);", 
                      "std::unique_ptr<LogicalPlanNode> planTableReference(binder::BoundTableReference* table_ref);\n    std::unique_ptr<LogicalPlanNode> planSelect(binder::BoundSelectStatement* stmt);")
    return c
rewrite("include/planner/planner.hpp", upd_planner_hpp)

# planner.cpp
plan_table_ref = """
std::unique_ptr<LogicalPlanNode> Planner::planTableReference(binder::BoundTableReference* table_ref) {
    if (auto base = dynamic_cast<binder::BoundBaseTableReference*>(table_ref)) {
        return std::make_unique<SeqScanPlanNode>(*base->schema_, base->table_name_, base->table_alias_);
    } else if (auto join = dynamic_cast<binder::BoundJoinTable*>(table_ref)) {
        auto left_node = planTableReference(join->left_.get());
        auto right_node = planTableReference(join->right_.get());
        
        std::vector<Column> cols;
        for (uint32_t i = 0; i < left_node->getOutputSchema().getColumnCount(); ++i) {
            cols.push_back(left_node->getOutputSchema().getColumn(i));
        }
        for (uint32_t i = 0; i < right_node->getOutputSchema().getColumnCount(); ++i) {
            cols.push_back(right_node->getOutputSchema().getColumn(i));
        }
        Schema join_schema(cols);
        
        std::unique_ptr<hamdb::Expression> condition = nullptr;
        if (join->condition_) {
            condition = join->condition_->takeExpr();
        }
        
        auto join_node = std::make_unique<LogicalNestedLoopJoinNode>(std::move(join_schema), std::move(condition));
        join_node->addChild(std::move(left_node));
        join_node->addChild(std::move(right_node));
        return join_node;
    }
    throw std::runtime_error("Unknown BoundTableReference type in planner");
}
"""
def upd_planner_cpp(c):
    if "Planner::planTableReference" not in c:
        c = c.replace('#include "planner/planner.hpp"', '#include "planner/planner.hpp"\n#include "planner/nested_loop_join_plan.hpp"')
        c = c.replace("std::unique_ptr<LogicalPlanNode> Planner::planSelect(binder::BoundSelectStatement* stmt) {", plan_table_ref + "\nstd::unique_ptr<LogicalPlanNode> Planner::planSelect(binder::BoundSelectStatement* stmt) {")
        
        # update planSelect FROM
        from_logic = """
    std::unique_ptr<LogicalPlanNode> current_node = nullptr;
    if (stmt->table_) {
        current_node = planTableReference(stmt->table_.get());
    } else {
        // dummy scan or values
        current_node = std::make_unique<ValuesPlanNode>(Schema({}), std::vector<std::vector<std::unique_ptr<hamdb::Expression>>>{});
    }
"""
        c = re.sub(r'TableInfo\* table_info = nullptr;[\s\S]*?std::unique_ptr<LogicalPlanNode> current_node = std::move\(seq_scan\);', from_logic, c)
    return c
rewrite("src/planner/planner.cpp", upd_planner_cpp)

# physical_planner.cpp
def upd_phys_planner_cpp(c):
    if "LogicalPlanType::NESTED_LOOP_JOIN:" not in c:
        c = c.replace('#include "planner/physical_planner.hpp"', '#include "planner/physical_planner.hpp"\n#include "planner/nested_loop_join_plan.hpp"')
        join_case = """
        case LogicalPlanType::NESTED_LOOP_JOIN: {
            auto* join_node = dynamic_cast<LogicalNestedLoopJoinNode*>(logical_node.get());
            physical_node = std::make_unique<NestedLoopJoinPlan>(
                join_node->getOutputSchema(), join_node->takePredicate());
            break;
        }"""
        c = c.replace('default:\n            throw std::runtime_error("Unsupported logical plan node type");', join_case + '\n        default:\n            throw std::runtime_error("Unsupported logical plan node type");')
    return c
rewrite("src/planner/physical_planner.cpp", upd_phys_planner_cpp)

# executor_factory.cpp
def upd_exec_factory(c):
    if "PhysicalPlanType::NESTED_LOOP_JOIN:" not in c:
        c = c.replace('#include "executor/executor_factory.hpp"', '#include "executor/executor_factory.hpp"\n#include "executor/nested_loop_join_executor.hpp"\n#include "planner/nested_loop_join_plan.hpp"')
        join_case = """
        case PhysicalPlanType::NESTED_LOOP_JOIN: {
            auto join_plan = dynamic_cast<const planner::NestedLoopJoinPlan*>(plan_node.get());
            auto left = createExecutor(std::move(children[0]), context);
            auto right = createExecutor(std::move(children[1]), context);
            executor = std::make_unique<NestedLoopJoinExecutor>(std::move(left), std::move(right), const_cast<planner::NestedLoopJoinPlan*>(join_plan)->getPredicate() ? const_cast<planner::NestedLoopJoinPlan*>(join_plan)->getPredicate()->clone() : nullptr);
            break;
        }"""
        c = c.replace('default:\n            throw std::runtime_error("Unsupported physical plan node type");', join_case + '\n        default:\n            throw std::runtime_error("Unsupported physical plan node type");')
    return c
rewrite("src/planner/executor_factory.cpp", upd_exec_factory)

# plan_formatter.cpp
def upd_fmt(c):
    if "LogicalPlanType::NESTED_LOOP_JOIN" not in c:
        c = c.replace('#include "planner/plan_formatter.hpp"', '#include "planner/plan_formatter.hpp"\n#include "planner/nested_loop_join_plan.hpp"')
        
        c = c.replace('case LogicalPlanType::INDEX_SCAN: {', 'case LogicalPlanType::NESTED_LOOP_JOIN: {\n            auto join = static_cast<const planner::LogicalNestedLoopJoinNode*>(node);\n            out += "NestedLoopJoin: {";\n            if (join->getPredicate()) out += "condition=" + join->getPredicate()->toString();\n            out += "}";\n            break;\n        }\n        case LogicalPlanType::INDEX_SCAN: {')
        
        c = c.replace('case PhysicalPlanType::INDEX_SCAN: {', 'case PhysicalPlanType::NESTED_LOOP_JOIN: {\n            auto join = static_cast<const planner::NestedLoopJoinPlan*>(node);\n            out += "NestedLoopJoin: {";\n            if (join->getPredicate()) out += "condition=" + join->getPredicate()->toString();\n            out += "}";\n            break;\n        }\n        case PhysicalPlanType::INDEX_SCAN: {')
    return c
rewrite("src/planner/plan_formatter.cpp", upd_fmt)

