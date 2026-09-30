#include "optimizer/join_selection_rule.hpp"
#include "executor/logical_expression.hpp"
#include "executor/comparison_expression.hpp"
#include "executor/column_value_expression.hpp"
#include "planner/nested_loop_join_plan.hpp"
#include "planner/hash_join_plan.hpp"
#include "planner/logical_plan.hpp"
#include <vector>

namespace hamdb::optimizer {

namespace {

    void SplitConjuncts(std::unique_ptr<hamdb::Expression> expr, std::vector<std::unique_ptr<hamdb::Expression>>& conjuncts) {
        if (!expr) return;
        if (auto* logic = dynamic_cast<hamdb::LogicalExpression*>(expr.get())) {
            if (logic->getLogicType() == hamdb::LogicalType::And) {
                auto left = std::move(logic->getMutableChildren()[0]);
                auto right = std::move(logic->getMutableChildren()[1]);
                SplitConjuncts(std::move(left), conjuncts);
                SplitConjuncts(std::move(right), conjuncts);
                return;
            }
        }
        conjuncts.push_back(std::move(expr));
    }

    std::unique_ptr<hamdb::Expression> CombineConjuncts(std::vector<std::unique_ptr<hamdb::Expression>>& conjuncts) {
        if (conjuncts.empty()) return nullptr;
        std::unique_ptr<hamdb::Expression> result = std::move(conjuncts[0]);
        for (size_t i = 1; i < conjuncts.size(); ++i) {
            result = std::make_unique<hamdb::LogicalExpression>(hamdb::LogicalType::And, std::move(result), std::move(conjuncts[i]));
        }
        return result;
    }

    void GetColumns(const hamdb::Expression* expr, std::vector<uint32_t>& cols) {
        if (!expr) return;
        if (auto* col_expr = dynamic_cast<const hamdb::ColumnValueExpression*>(expr)) {
            cols.push_back(col_expr->getColIdx());
        }
        for (const auto& child : expr->getChildren()) {
            GetColumns(child.get(), cols);
        }
    }

    std::unique_ptr<hamdb::Expression> ShiftExpression(std::unique_ptr<hamdb::Expression> expr, int offset) {
        if (!expr) return nullptr;
        
        if (auto* col = dynamic_cast<hamdb::ColumnValueExpression*>(expr.get())) {
            // Need to create a new ColumnValueExpression but there is no getTupleSource()
            // However, the rule only relies on col_idx. 
            // We can just construct a new one with the shifted offset.
            return std::make_unique<hamdb::ColumnValueExpression>(col->getColIdx() + offset);
        }
        
        auto& children = expr->getMutableChildren();
        for (size_t i = 0; i < children.size(); ++i) {
            children[i] = ShiftExpression(std::move(children[i]), offset);
        }
        return expr;
    }

} // namespace

std::unique_ptr<planner::LogicalPlanNode> JoinSelectionRule::apply(std::unique_ptr<planner::LogicalPlanNode> plan) {
    if (!plan) return nullptr;

    // Apply rule bottom-up
    auto& children = plan->getChildren();
    for (auto& child : children) {
        child = apply(std::move(child));
    }

    if (plan->getType() == planner::LogicalPlanType::NESTED_LOOP_JOIN) {
        auto* join_node = dynamic_cast<planner::LogicalNestedLoopJoinNode*>(plan.get());
        if (join_node != nullptr && join_node->getChildren().size() == 2) {
            auto& left_child = join_node->getChildren()[0];
            uint32_t left_cols = left_child->getOutputSchema().getColumnCount();
            
            std::vector<std::unique_ptr<hamdb::Expression>> conjuncts;
            SplitConjuncts(join_node->takePredicate(), conjuncts);
            
            std::unique_ptr<hamdb::Expression> hash_left_key = nullptr;
            std::unique_ptr<hamdb::Expression> hash_right_key = nullptr;
            
            auto it = conjuncts.begin();
            while (it != conjuncts.end()) {
                if (auto* comp = dynamic_cast<hamdb::ComparisonExpression*>(it->get())) {
                    if (comp->getComparisonType() == hamdb::ComparisonType::Equal) {
                        auto& left_expr = comp->getMutableChildren()[0];
                        auto& right_expr = comp->getMutableChildren()[1];
                        
                        std::vector<uint32_t> left_expr_cols;
                        std::vector<uint32_t> right_expr_cols;
                        
                        GetColumns(left_expr.get(), left_expr_cols);
                        GetColumns(right_expr.get(), right_expr_cols);
                        
                        bool left_is_all_left = true;
                        bool left_is_all_right = true;
                        for (uint32_t c : left_expr_cols) {
                            if (c >= left_cols) left_is_all_left = false;
                            if (c < left_cols) left_is_all_right = false;
                        }
                        
                        bool right_is_all_left = true;
                        bool right_is_all_right = true;
                        for (uint32_t c : right_expr_cols) {
                            if (c >= left_cols) right_is_all_left = false;
                            if (c < left_cols) right_is_all_right = false;
                        }
                        
                        // We found an equi-join condition!
                        if (left_is_all_left && right_is_all_right && !left_expr_cols.empty() && !right_expr_cols.empty()) {
                            hash_left_key = std::move(left_expr);
                            hash_right_key = ShiftExpression(std::move(right_expr), -static_cast<int>(left_cols));
                            it = conjuncts.erase(it);
                            break;
                        }
                        
                        if (left_is_all_right && right_is_all_left && !left_expr_cols.empty() && !right_expr_cols.empty()) {
                            hash_left_key = std::move(right_expr);
                            hash_right_key = ShiftExpression(std::move(left_expr), -static_cast<int>(left_cols));
                            it = conjuncts.erase(it);
                            break;
                        }
                    }
                }
                ++it;
            }
            
            if (hash_left_key && hash_right_key) {
                auto hash_join = std::make_unique<planner::LogicalHashJoinNode>(
                    join_node->getOutputSchema(),
                    std::move(hash_left_key),
                    std::move(hash_right_key)
                );
                
                hash_join->addChild(std::move(join_node->getChildren()[0]));
                hash_join->addChild(std::move(join_node->getChildren()[1]));
                
                if (!conjuncts.empty()) {
                    auto residual_pred = CombineConjuncts(conjuncts);
                    auto filter = std::make_unique<planner::FilterPlanNode>(
                        hash_join->getOutputSchema(),
                        std::move(residual_pred)
                    );
                    filter->addChild(std::move(hash_join));
                    return filter;
                }
                
                return hash_join;
            }
            
            // Restore predicate if no equi-join is found
            join_node->setPredicate(CombineConjuncts(conjuncts));
        }
    }
    
    return plan;
}

} // namespace hamdb::optimizer
