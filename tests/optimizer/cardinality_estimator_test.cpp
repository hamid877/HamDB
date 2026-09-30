#include <gtest/gtest.h>
#include "optimizer/cardinality_estimator.hpp"
#include "executor/column_value_expression.hpp"
#include "executor/constant_expression.hpp"
#include "executor/comparison_expression.hpp"

using namespace hamdb;

class CardinalityEstimatorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        stats_manager_ = std::make_unique<StatisticsManager>(nullptr, nullptr);
    }
    
    std::unique_ptr<StatisticsManager> stats_manager_;
};

TEST_F(CardinalityEstimatorTest, EqualitySelectivity)
{
    CardinalityEstimator estimator(stats_manager_.get());
    
    TableStatistics stats;
    stats.row_count = 100;
    ColumnStatistics col_stat;
    col_stat.distinct_count = 10;
    stats.column_stats.push_back(col_stat);
    
    Schema schema;
    
    auto col_expr = std::make_unique<ColumnValueExpression>(0);
    auto const_expr = std::make_unique<ConstantExpression>(Value(5));
    auto comp = std::make_unique<ComparisonExpression>(ComparisonType::Equal, std::move(col_expr), std::move(const_expr));
    
    double sel = estimator.estimateSelectivity(comp.get(), stats, schema);
    EXPECT_DOUBLE_EQ(sel, 0.1);
}

TEST_F(CardinalityEstimatorTest, RangeSelectivity)
{
    CardinalityEstimator estimator(stats_manager_.get());
    
    TableStatistics stats;
    stats.row_count = 100;
    ColumnStatistics col_stat;
    col_stat.distinct_count = 10;
    col_stat.min_value = Value(0);
    col_stat.max_value = Value(100);
    stats.column_stats.push_back(col_stat);
    
    Schema schema;
    
    auto col_expr = std::make_unique<ColumnValueExpression>(0);
    auto const_expr = std::make_unique<ConstantExpression>(Value(25));
    auto comp = std::make_unique<ComparisonExpression>(ComparisonType::LessThan, std::move(col_expr), std::move(const_expr));
    
    double sel = estimator.estimateSelectivity(comp.get(), stats, schema);
    EXPECT_DOUBLE_EQ(sel, 0.25);
}

TEST_F(CardinalityEstimatorTest, JoinSelectivity)
{
    CardinalityEstimator estimator(stats_manager_.get());
    
    TableStatistics left_stats;
    ColumnStatistics left_col_stat;
    left_col_stat.distinct_count = 50;
    left_stats.column_stats.push_back(left_col_stat);
    
    TableStatistics right_stats;
    ColumnStatistics right_col_stat;
    right_col_stat.distinct_count = 200;
    right_stats.column_stats.push_back(right_col_stat);
    
    Schema schema;
    
    auto col_expr1 = std::make_unique<ColumnValueExpression>(0);
    auto col_expr2 = std::make_unique<ColumnValueExpression>(0);
    auto comp = std::make_unique<ComparisonExpression>(ComparisonType::Equal, std::move(col_expr1), std::move(col_expr2));
    
    double sel = estimator.estimateJoinSelectivity(comp.get(), left_stats, schema, right_stats, schema);
    EXPECT_DOUBLE_EQ(sel, 0.005); // 1.0 / max(50, 200)
}
