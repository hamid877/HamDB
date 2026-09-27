#include "optimizer/index_scan_rule.hpp"
#include "planner/logical_plan.hpp"
#include "planner/logical_index_scan.hpp"
#include "executor/comparison_expression.hpp"
#include "executor/column_value_expression.hpp"
#include "executor/constant_expression.hpp"

namespace hamdb::optimizer {

std::unique_ptr<planner::LogicalPlanNode> IndexScanRule::apply(std::unique_ptr<planner::LogicalPlanNode> plan) {
    for (auto& child : plan->getChildren()) {
        child = apply(std::move(child));
    }
    
    if (plan->getType() == planner::LogicalPlanType::SEQ_SCAN) {
        auto* seq_scan = dynamic_cast<planner::SeqScanPlanNode*>(plan.get());
        if (seq_scan->getPredicate()) {
            auto* comp_expr = dynamic_cast<ComparisonExpression*>(const_cast<Expression*>(seq_scan->getPredicate()));
            if (comp_expr) {
                auto comp_type = comp_expr->getComparisonType();
                if (comp_type == ComparisonType::Equal || comp_type == ComparisonType::LessThan || 
                    comp_type == ComparisonType::LessThanOrEqual || comp_type == ComparisonType::GreaterThan ||
                    comp_type == ComparisonType::GreaterThanOrEqual) {
                    
                    if (comp_expr->getChildren().size() == 2) {
                        auto* col_expr = dynamic_cast<ColumnValueExpression*>(comp_expr->getChildren()[0].get());
                        auto* const_expr = dynamic_cast<ConstantExpression*>(comp_expr->getChildren()[1].get());
                        
                        if (col_expr && const_expr && col_expr->getColIdx() == 0) {
                            TableInfo* table_info = nullptr;
                            if (catalog_->getTable(seq_scan->getTableName(), table_info) == Status::Ok) {
                                if (table_info->getIndexRootPage() != kInvalidPageId) {
                                    auto new_scan = std::make_unique<planner::LogicalIndexScanNode>(
                                        seq_scan->getOutputSchema(), seq_scan->getTableName(), seq_scan->getTableAlias(),
                                        seq_scan->takePredicate()
                                    );
                                    return new_scan;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    return plan;
}

} // namespace hamdb::optimizer
