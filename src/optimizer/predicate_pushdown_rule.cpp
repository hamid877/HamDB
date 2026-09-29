#include "optimizer/predicate_pushdown_rule.hpp"
#include "executor/logical_expression.hpp"
#include "executor/column_value_expression.hpp"
#include "planner/nested_loop_join_plan.hpp"
#include "planner/hash_join_plan.hpp"
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
            return std::make_unique<hamdb::ColumnValueExpression>(col->getColIdx() + offset);
        }
        
        auto& children = expr->getMutableChildren();
        for (size_t i = 0; i < children.size(); ++i) {
            children[i] = ShiftExpression(std::move(children[i]), offset);
        }
        return expr;
    }

} // namespace

std::unique_ptr<planner::LogicalPlanNode> PredicatePushdownRule::apply(std::unique_ptr<planner::LogicalPlanNode> plan) {
    if (!plan) return nullptr;

    if (plan->getType() == planner::LogicalPlanType::FILTER) {
        auto* filter_node = dynamic_cast<planner::FilterPlanNode*>(plan.get());
        if (filter_node != nullptr && filter_node->getChildren().size() == 1) {
            auto& child = filter_node->getChildren()[0];
            
            if (child->getType() == planner::LogicalPlanType::SEQ_SCAN) {
                auto* seq_scan = dynamic_cast<planner::SeqScanPlanNode*>(child.get());
                if (seq_scan != nullptr) {
                    auto filter_pred = filter_node->takePredicate();
                    if (seq_scan->getPredicate() != nullptr) {
                        auto old_pred = seq_scan->takePredicate();
                        auto and_expr = std::make_unique<LogicalExpression>(LogicalType::And, std::move(old_pred), std::move(filter_pred));
                        seq_scan->setPredicate(std::move(and_expr));
                    } else {
                        seq_scan->setPredicate(std::move(filter_pred));
                    }
                    return std::move(child);
                }
            } else if (child->getType() == planner::LogicalPlanType::NESTED_LOOP_JOIN || child->getType() == planner::LogicalPlanType::HASH_JOIN) {
                uint32_t left_cols = child->getChildren()[0]->getOutputSchema().getColumnCount();
                
                std::vector<std::unique_ptr<hamdb::Expression>> conjuncts;
                SplitConjuncts(filter_node->takePredicate(), conjuncts);
                
                std::vector<std::unique_ptr<hamdb::Expression>> left_conjuncts;
                std::vector<std::unique_ptr<hamdb::Expression>> right_conjuncts;
                std::vector<std::unique_ptr<hamdb::Expression>> remain_conjuncts;
                
                for (auto& conj : conjuncts) {
                    std::vector<uint32_t> cols;
                    GetColumns(conj.get(), cols);
                    
                    bool all_left = true;
                    bool all_right = true;
                    
                    for (uint32_t c : cols) {
                        if (c >= left_cols) all_left = false;
                        if (c < left_cols) all_right = false;
                    }
                    
                    if (all_left && !cols.empty()) {
                        left_conjuncts.push_back(std::move(conj));
                    } else if (all_right && !cols.empty()) {
                        right_conjuncts.push_back(ShiftExpression(std::move(conj), -static_cast<int>(left_cols)));
                    } else {
                        remain_conjuncts.push_back(std::move(conj));
                    }
                }
                
                if (!left_conjuncts.empty()) {
                    auto left_pred = CombineConjuncts(left_conjuncts);
                    auto new_left_filter = std::make_unique<planner::FilterPlanNode>(child->getChildren()[0]->getOutputSchema(), std::move(left_pred));
                    new_left_filter->addChild(std::move(child->getChildren()[0]));
                    child->getChildren()[0] = apply(std::move(new_left_filter));
                }
                
                if (!right_conjuncts.empty()) {
                    auto right_pred = CombineConjuncts(right_conjuncts);
                    auto new_right_filter = std::make_unique<planner::FilterPlanNode>(child->getChildren()[1]->getOutputSchema(), std::move(right_pred));
                    new_right_filter->addChild(std::move(child->getChildren()[1]));
                    child->getChildren()[1] = apply(std::move(new_right_filter));
                }
                
                if (!remain_conjuncts.empty()) {
                    auto remain_pred = CombineConjuncts(remain_conjuncts);
                    filter_node->setPredicate(std::move(remain_pred));
                    return plan;
                } else {
                    return std::move(child);
                }
            }
        }
    }
    
    return plan;
}

} // namespace hamdb::optimizer
