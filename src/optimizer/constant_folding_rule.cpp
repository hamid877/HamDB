#include "optimizer/constant_folding_rule.hpp"
#include "executor/constant_expression.hpp"
#include "executor/arithmetic_expression.hpp"
#include "executor/comparison_expression.hpp"
#include "executor/logical_expression.hpp"
#include "planner/logical_index_scan.hpp"

namespace hamdb::optimizer {

std::unique_ptr<hamdb::Expression> ConstantFoldingRule::foldExpression(std::unique_ptr<hamdb::Expression> expr) {
    if (!expr) return nullptr;

    auto& children = expr->getMutableChildren();
    for (auto& child : children) {
        child = foldExpression(std::move(child));
    }

    if (auto* arith = dynamic_cast<ArithmeticExpression*>(expr.get())) {
        auto* left = dynamic_cast<ConstantExpression*>(children[0].get());
        auto* right = dynamic_cast<ConstantExpression*>(children[1].get());
        if (left && right) {
            Tuple dummy_tuple;
            Schema dummy_schema;
            Value result = arith->evaluate(dummy_tuple, dummy_schema);
            return std::make_unique<ConstantExpression>(result);
        }
    } else if (auto* comp = dynamic_cast<ComparisonExpression*>(expr.get())) {
        auto* left = dynamic_cast<ConstantExpression*>(children[0].get());
        auto* right = dynamic_cast<ConstantExpression*>(children[1].get());
        if (left && right) {
            Tuple dummy_tuple;
            Schema dummy_schema;
            Value result = comp->evaluate(dummy_tuple, dummy_schema);
            return std::make_unique<ConstantExpression>(result);
        }
    } else if (auto* logic = dynamic_cast<LogicalExpression*>(expr.get())) {
        if (logic->getLogicType() == LogicalType::Not) {
            auto* child = dynamic_cast<ConstantExpression*>(children[0].get());
            if (child) {
                Tuple dummy_tuple;
                Schema dummy_schema;
                Value result = logic->evaluate(dummy_tuple, dummy_schema);
                return std::make_unique<ConstantExpression>(result);
            }
        } else {
            auto* left_const = dynamic_cast<ConstantExpression*>(children[0].get());
            auto* right_const = dynamic_cast<ConstantExpression*>(children[1].get());

            if (left_const && right_const) {
                Tuple dummy_tuple;
                Schema dummy_schema;
                Value result = logic->evaluate(dummy_tuple, dummy_schema);
                return std::make_unique<ConstantExpression>(result);
            }

            if (logic->getLogicType() == LogicalType::And) {
                if (left_const && !left_const->evaluate({}, {}).isNull()) {
                    if (left_const->evaluate({}, {}).getAsBoolean()) {
                        return std::move(children[1]);
                    } else {
                        return std::make_unique<ConstantExpression>(Value(false));
                    }
                }
                if (right_const && !right_const->evaluate({}, {}).isNull()) {
                    if (right_const->evaluate({}, {}).getAsBoolean()) {
                        return std::move(children[0]);
                    } else {
                        return std::make_unique<ConstantExpression>(Value(false));
                    }
                }
            } else if (logic->getLogicType() == LogicalType::Or) {
                if (left_const && !left_const->evaluate({}, {}).isNull()) {
                    if (left_const->evaluate({}, {}).getAsBoolean()) {
                        return std::make_unique<ConstantExpression>(Value(true));
                    } else {
                        return std::move(children[1]);
                    }
                }
                if (right_const && !right_const->evaluate({}, {}).isNull()) {
                    if (right_const->evaluate({}, {}).getAsBoolean()) {
                        return std::make_unique<ConstantExpression>(Value(true));
                    } else {
                        return std::move(children[0]);
                    }
                }
            }
        }
    }

    return expr;
}

std::unique_ptr<planner::LogicalPlanNode> ConstantFoldingRule::apply(std::unique_ptr<planner::LogicalPlanNode> plan) {
    if (!plan) return nullptr;

    switch (plan->getType()) {
        case planner::LogicalPlanType::SEQ_SCAN: {
            auto* node = dynamic_cast<planner::SeqScanPlanNode*>(plan.get());
            if (node && node->getPredicate()) {
                node->setPredicate(foldExpression(node->takePredicate()));
            }
            break;
        }
        case planner::LogicalPlanType::NESTED_LOOP_JOIN:
        case planner::LogicalPlanType::INDEX_SCAN: {
            auto* node = dynamic_cast<planner::LogicalIndexScanNode*>(plan.get());
            if (node && node->getPredicate()) {
                node->predicate_ = foldExpression(node->takePredicate());
            }
            break;
        }
        case planner::LogicalPlanType::FILTER: {
            auto* node = dynamic_cast<planner::FilterPlanNode*>(plan.get());
            if (node && node->getPredicate()) {
                node->setPredicate(foldExpression(node->takePredicate()));
            }
            break;
        }
        case planner::LogicalPlanType::PROJECTION: {
            auto* node = dynamic_cast<planner::ProjectionPlanNode*>(plan.get());
            if (node) {
                for (auto& expr : node->getMutableExpressions()) {
                    expr = foldExpression(std::move(expr));
                }
            }
            break;
        }
        case planner::LogicalPlanType::SORT: {
            auto* node = dynamic_cast<planner::SortPlanNode*>(plan.get());
            if (node) {
                for (auto& pair : node->getMutableOrderBy()) {
                    pair.first = foldExpression(std::move(pair.first));
                }
            }
            break;
        }
        case planner::LogicalPlanType::LIMIT: {
            auto* node = dynamic_cast<planner::LimitPlanNode*>(plan.get());
            if (node) {
                if (node->getLimit()) {
                    node->setLimit(foldExpression(node->takeLimit()));
                }
                if (node->getOffset()) {
                    node->setOffset(foldExpression(node->takeOffset()));
                }
            }
            break;
        }
        case planner::LogicalPlanType::VALUES: {
            auto* node = dynamic_cast<planner::ValuesPlanNode*>(plan.get());
            if (node) {
                for (auto& row : node->getMutableValues()) {
                    for (auto& expr : row) {
                        expr = foldExpression(std::move(expr));
                    }
                }
            }
            break;
        }
        case planner::LogicalPlanType::UPDATE: {
            auto* node = dynamic_cast<planner::UpdatePlanNode*>(plan.get());
            if (node) {
                for (auto& pair : node->getMutableSetClauses()) {
                    pair.second = foldExpression(std::move(pair.second));
                }
            }
            break;
        }
        case planner::LogicalPlanType::INSERT:
        case planner::LogicalPlanType::DELETE:
            break;
    }
    return plan;
}

} // namespace hamdb::optimizer
