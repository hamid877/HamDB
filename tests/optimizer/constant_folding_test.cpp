#include "optimizer/rule_executor.hpp"
#include "optimizer/constant_folding_rule.hpp"
#include "planner/logical_plan.hpp"
#include "executor/constant_expression.hpp"
#include "executor/arithmetic_expression.hpp"
#include "executor/comparison_expression.hpp"
#include "executor/logical_expression.hpp"
#include "executor/column_value_expression.hpp"
#include <gtest/gtest.h>

namespace hamdb::optimizer {

class ConstantFoldingTest : public ::testing::Test {
protected:
    void SetUp() override {
        rule_executor_ = std::make_unique<RuleExecutor>();
        rule_executor_->addRule(std::make_unique<ConstantFoldingRule>());
    }

    std::unique_ptr<RuleExecutor> rule_executor_;
};

TEST_F(ConstantFoldingTest, FoldArithmetic) {
    auto left = std::make_unique<hamdb::ConstantExpression>(Value(2));
    auto right = std::make_unique<hamdb::ConstantExpression>(Value(3));
    auto add = std::make_unique<hamdb::ArithmeticExpression>(hamdb::ArithmeticType::Add, std::move(left), std::move(right));
    
    Schema empty_schema;
    auto filter = std::make_unique<planner::FilterPlanNode>(empty_schema, std::move(add));
    
    std::unique_ptr<planner::LogicalPlanNode> root = std::move(filter);
    root = rule_executor_->optimize(std::move(root));
    
    auto* opt_filter = dynamic_cast<planner::FilterPlanNode*>(root.get());
    ASSERT_NE(opt_filter, nullptr);
    
    auto* folded = dynamic_cast<const hamdb::ConstantExpression*>(opt_filter->getPredicate());
    ASSERT_NE(folded, nullptr);
    
    Tuple dummy;
    EXPECT_EQ(folded->evaluate(dummy, empty_schema).getAsInteger(), 5);
}

TEST_F(ConstantFoldingTest, FoldComparison) {
    auto left = std::make_unique<hamdb::ConstantExpression>(Value(5));
    auto right = std::make_unique<hamdb::ConstantExpression>(Value(5));
    auto eq = std::make_unique<hamdb::ComparisonExpression>(hamdb::ComparisonType::Equal, std::move(left), std::move(right));
    
    Schema empty_schema;
    auto filter = std::make_unique<planner::FilterPlanNode>(empty_schema, std::move(eq));
    
    std::unique_ptr<planner::LogicalPlanNode> root = std::move(filter);
    root = rule_executor_->optimize(std::move(root));
    
    auto* opt_filter = dynamic_cast<planner::FilterPlanNode*>(root.get());
    auto* folded = dynamic_cast<const hamdb::ConstantExpression*>(opt_filter->getPredicate());
    ASSERT_NE(folded, nullptr);
    
    Tuple dummy;
    EXPECT_TRUE(folded->evaluate(dummy, empty_schema).getAsBoolean());
}

TEST_F(ConstantFoldingTest, BooleanIdentityAndTrue) {
    auto left = std::make_unique<hamdb::ConstantExpression>(Value(true));
    auto right = std::make_unique<hamdb::ColumnValueExpression>(0); // Dummy non-const
    auto and_expr = std::make_unique<hamdb::LogicalExpression>(hamdb::LogicalType::And, std::move(left), std::move(right));
    
    Schema empty_schema;
    auto filter = std::make_unique<planner::FilterPlanNode>(empty_schema, std::move(and_expr));
    
    std::unique_ptr<planner::LogicalPlanNode> root = std::move(filter);
    root = rule_executor_->optimize(std::move(root));
    
    auto* opt_filter = dynamic_cast<planner::FilterPlanNode*>(root.get());
    auto* folded = dynamic_cast<const hamdb::ColumnValueExpression*>(opt_filter->getPredicate());
    ASSERT_NE(folded, nullptr); // Should reduce to just the non-const expression
}

TEST_F(ConstantFoldingTest, BooleanIdentityAndFalse) {
    auto left = std::make_unique<hamdb::ConstantExpression>(Value(false));
    auto right = std::make_unique<hamdb::ColumnValueExpression>(0); // Dummy non-const
    auto and_expr = std::make_unique<hamdb::LogicalExpression>(hamdb::LogicalType::And, std::move(left), std::move(right));
    
    Schema empty_schema;
    auto filter = std::make_unique<planner::FilterPlanNode>(empty_schema, std::move(and_expr));
    
    std::unique_ptr<planner::LogicalPlanNode> root = std::move(filter);
    root = rule_executor_->optimize(std::move(root));
    
    auto* opt_filter = dynamic_cast<planner::FilterPlanNode*>(root.get());
    auto* folded = dynamic_cast<const hamdb::ConstantExpression*>(opt_filter->getPredicate());
    ASSERT_NE(folded, nullptr); 
    Tuple dummy;
    EXPECT_FALSE(folded->evaluate(dummy, empty_schema).getAsBoolean());
}

} // namespace hamdb::optimizer
