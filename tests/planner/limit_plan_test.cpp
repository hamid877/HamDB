// SPDX-License-Identifier: MIT
// M8.6 — LIMIT/OFFSET Plan structural unit tests.

#include "planner/logical_plan.hpp"
#include "planner/physical_plan.hpp"
#include "executor/column_value_expression.hpp"
#include "executor/constant_expression.hpp"

#include <gtest/gtest.h>
#include <memory>

namespace hamdb::planner::test {

// ---------------------------------------------------------------------------
// LogicalLimitNode (LimitPlanNode) structural tests
// ---------------------------------------------------------------------------

TEST(LimitPlanTest, LogicalLimitNodeCreation)
{
    Schema schema({Column{"a", ColumnType::Integer}});
    auto limit_expr  = std::make_unique<ConstantExpression>(Value(10));
    auto offset_expr = std::make_unique<ConstantExpression>(Value(0));

    LimitPlanNode node(schema, std::move(limit_expr), std::move(offset_expr));

    EXPECT_EQ(node.getType(), LogicalPlanType::LIMIT);
    ASSERT_NE(node.getLimit(),  nullptr);
    ASSERT_NE(node.getOffset(), nullptr);
}

TEST(LimitPlanTest, LogicalLimitNodeEvaluatesLimit)
{
    Schema schema({Column{"a", ColumnType::Integer}});
    auto limit_expr  = std::make_unique<ConstantExpression>(Value(5));
    auto offset_expr = std::make_unique<ConstantExpression>(Value(2));

    LimitPlanNode node(schema, std::move(limit_expr), std::move(offset_expr));

    Schema empty_schema(std::vector<Column>{});
    Tuple  empty_tuple;

    int32_t limit_val  = node.getLimit()->evaluate(empty_tuple, empty_schema).getAsInteger();
    int32_t offset_val = node.getOffset()->evaluate(empty_tuple, empty_schema).getAsInteger();

    EXPECT_EQ(limit_val,  5);
    EXPECT_EQ(offset_val, 2);
}

TEST(LimitPlanTest, LogicalLimitNodeZeroLimit)
{
    Schema schema({Column{"a", ColumnType::Integer}});
    auto limit_expr  = std::make_unique<ConstantExpression>(Value(0));
    auto offset_expr = std::make_unique<ConstantExpression>(Value(0));

    LimitPlanNode node(schema, std::move(limit_expr), std::move(offset_expr));

    Schema empty_schema(std::vector<Column>{});
    Tuple  empty_tuple;

    int32_t limit_val = node.getLimit()->evaluate(empty_tuple, empty_schema).getAsInteger();
    EXPECT_EQ(limit_val, 0);
}

// ---------------------------------------------------------------------------
// Physical LimitPlan structural tests
// ---------------------------------------------------------------------------

TEST(LimitPlanTest, PhysicalLimitPlanCreation)
{
    Schema schema({Column{"a", ColumnType::Integer}});

    LimitPlan plan(schema, 10, 0);

    EXPECT_EQ(plan.getType(),   PhysicalPlanType::LIMIT);
    EXPECT_EQ(plan.getLimit(),  10u);
    EXPECT_EQ(plan.getOffset(), 0u);
}

TEST(LimitPlanTest, PhysicalLimitPlanWithOffset)
{
    Schema schema({Column{"a", ColumnType::Integer}});

    LimitPlan plan(schema, 5, 3);

    EXPECT_EQ(plan.getType(),   PhysicalPlanType::LIMIT);
    EXPECT_EQ(plan.getLimit(),  5u);
    EXPECT_EQ(plan.getOffset(), 3u);
}

TEST(LimitPlanTest, PhysicalLimitPlanZeroLimit)
{
    Schema schema({Column{"a", ColumnType::Integer}});

    LimitPlan plan(schema, 0, 0);

    EXPECT_EQ(plan.getLimit(),  0u);
    EXPECT_EQ(plan.getOffset(), 0u);
}

TEST(LimitPlanTest, PhysicalLimitPlanOutputSchema)
{
    Schema schema({Column{"id", ColumnType::Integer}, Column{"name", ColumnType::Varchar}});

    LimitPlan plan(schema, 3, 1);

    EXPECT_EQ(plan.getOutputSchema().getColumnCount(), 2u);
    EXPECT_EQ(plan.getOutputSchema().getColumn(0).getName(), "id");
    EXPECT_EQ(plan.getOutputSchema().getColumn(1).getName(), "name");
}

} // namespace hamdb::planner::test
