import sys

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

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
    c = c.replace('#include "planner/planner.hpp"', '#include "planner/planner.hpp"\n#include "planner/nested_loop_join_plan.hpp"')
    c = c.replace("std::unique_ptr<LogicalPlanNode> Planner::planSelect(binder::BoundSelectStatement* stmt) {", plan_table_ref + "\nstd::unique_ptr<LogicalPlanNode> Planner::planSelect(binder::BoundSelectStatement* stmt) {")
    
    from_logic = """
    std::unique_ptr<LogicalPlanNode> current_node = nullptr;
    if (stmt->table_) {
        current_node = planTableReference(stmt->table_.get());
    } else {
        std::vector<Column> cols;
        current_node = std::make_unique<ValuesPlanNode>(Schema(cols), std::vector<std::vector<std::unique_ptr<hamdb::Expression>>>{});
    }
"""
    
    old_from = """    TableInfo* table_info = nullptr;
    if (catalog_->getTable(stmt->table_name_, table_info) != Status::Ok) {
        throw std::runtime_error("Table not found: " + stmt->table_name_);
    }
    
    // FROM: SeqScan
    auto seq_scan = std::make_unique<SeqScanPlanNode>(table_info->getSchema(), stmt->table_name_, stmt->table_alias_);
    std::unique_ptr<LogicalPlanNode> current_node = std::move(seq_scan);"""
    
    c = c.replace(old_from, from_logic, 1)  # Only replace the first occurrence!
    return c

rewrite("src/planner/planner.cpp", upd_planner_cpp)
