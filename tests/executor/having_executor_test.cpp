#include "executor/having_executor.hpp"
#include "executor/values_executor.hpp"
#include "executor/executor_context.hpp"
#include "executor/constant_expression.hpp"
#include "executor/comparison_expression.hpp"
#include "executor/column_value_expression.hpp"
#include <gtest/gtest.h>
#include <memory>

namespace hamdb::executor {

class HavingExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Mock execution context (no real DB needed for having test)
        exec_ctx_ = std::make_unique<ExecutorContext>(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        
        std::vector<Column> cols = {
            Column("id", ColumnType::Integer),
            Column("count", ColumnType::Integer)
        };
        schema_ = std::make_unique<Schema>(cols);
    }
    
    std::unique_ptr<ExecutorContext> exec_ctx_;
    std::unique_ptr<Schema> schema_;
};

TEST_F(HavingExecutorTest, FilterGroups) {
    std::vector<std::vector<std::unique_ptr<Expression>>> values;
    
    std::vector<std::unique_ptr<Expression>> row1;
    row1.push_back(std::make_unique<ConstantExpression>(Value(1)));
    row1.push_back(std::make_unique<ConstantExpression>(Value(5)));
    values.push_back(std::move(row1));
    
    std::vector<std::unique_ptr<Expression>> row2;
    row2.push_back(std::make_unique<ConstantExpression>(Value(2)));
    row2.push_back(std::make_unique<ConstantExpression>(Value(15)));
    values.push_back(std::move(row2));
    
    auto child = std::make_unique<ValuesExecutor>(std::move(values), *schema_);
    
    // count > 10 (idx 1 > 10)
    auto predicate = std::make_unique<ComparisonExpression>(
        ComparisonType::GreaterThan,
        std::make_unique<ColumnValueExpression>(1),
        std::make_unique<ConstantExpression>(Value(10))
    );
    
    auto plan = std::make_unique<planner::HavingPlan>(*schema_, std::move(predicate));
    HavingExecutor executor(exec_ctx_.get(), plan.get(), std::move(child));
    executor.init();
    
    Tuple tuple;
    RID rid;
    
    bool has_next = executor.next(&tuple, &rid);
    ASSERT_TRUE(has_next);
    EXPECT_EQ(ColumnValueExpression(0).evaluate(tuple, *schema_).getAsInteger(), 2);
    EXPECT_EQ(ColumnValueExpression(1).evaluate(tuple, *schema_).getAsInteger(), 15);
    
    has_next = executor.next(&tuple, &rid);
    ASSERT_FALSE(has_next);
}

} // namespace hamdb::executor
