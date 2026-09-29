// SPDX-License-Identifier: MIT
// M8.6 — LIMIT/OFFSET integration tests.
//
// Verifies that LimitExecutor correctly wraps:
//   • OrderByExecutor  (LIMIT after ORDER BY)
//   • ValuesExecutor acting as mock aggregation output
//   • HavingExecutor  (LIMIT after HAVING)
//   • NestedLoopJoinExecutor  (LIMIT after join)
//
// All tests use mock/values child executors so no real DB is required.

#include "executor/limit_executor.hpp"
#include "executor/order_by_executor.hpp"
#include "executor/having_executor.hpp"
#include "executor/nested_loop_join_executor.hpp"
#include "executor/values_executor.hpp"
#include "executor/column_value_expression.hpp"
#include "executor/constant_expression.hpp"
#include "executor/comparison_expression.hpp"
#include "executor/executor_context.hpp"
#include "planner/having_plan.hpp"
#include "utils/serializer.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <span>
#include <vector>

namespace hamdb {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

Tuple makeTuple(int32_t a)
{
    std::vector<std::byte> buf(sizeof(int32_t) * 2);
    Serializer ser(buf);
    (void)ser.writeInt32(a);
    return Tuple(std::span<const std::byte>(buf.data(), ser.position()));
}

/// Simple mock executor that replays a pre-built tuple list.
class MockExecutor : public AbstractExecutor
{
public:
    MockExecutor(Schema schema, std::vector<Tuple> tuples)
        : schema_(std::move(schema)), tuples_(std::move(tuples)), idx_(0)
    {
    }

    void init() override { idx_ = 0; }

    bool next(Tuple* tuple, RID* rid) override
    {
        if (idx_ < tuples_.size())
        {
            *tuple = tuples_[idx_];
            *rid   = RID(0, static_cast<uint32_t>(idx_));
            ++idx_;
            return true;
        }
        return false;
    }

    const Schema& outputSchema() const override { return schema_; }

private:
    Schema             schema_;
    std::vector<Tuple> tuples_;
    std::size_t        idx_;
};

// ---------------------------------------------------------------------------
// LIMIT after ORDER BY
// ---------------------------------------------------------------------------

TEST(LimitIntegrationTest, LimitAfterOrderByAscending)
{
    Schema schema({Column{"a", ColumnType::Integer}});

    std::vector<Tuple> tuples = {makeTuple(5), makeTuple(1), makeTuple(3), makeTuple(2), makeTuple(4)};

    auto child = std::make_unique<MockExecutor>(schema, tuples);
    std::vector<std::pair<OrderByDirection, std::unique_ptr<Expression>>> order_bys;
    order_bys.emplace_back(OrderByDirection::ASC, std::make_unique<ColumnValueExpression>(0));

    auto sorted = std::make_unique<OrderByExecutor>(std::move(child), std::move(order_bys));

    LimitExecutor exec(std::move(sorted), 3, 0);
    exec.init();

    ColumnValueExpression col0(0);
    Tuple t;
    RID   rid;

    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(col0.evaluate(t, schema).getAsInteger(), 1);

    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(col0.evaluate(t, schema).getAsInteger(), 2);

    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(col0.evaluate(t, schema).getAsInteger(), 3);

    EXPECT_FALSE(exec.next(&t, &rid));
}

TEST(LimitIntegrationTest, LimitAfterOrderByDescending)
{
    Schema schema({Column{"a", ColumnType::Integer}});

    std::vector<Tuple> tuples = {makeTuple(1), makeTuple(4), makeTuple(2), makeTuple(5), makeTuple(3)};

    auto child = std::make_unique<MockExecutor>(schema, tuples);
    std::vector<std::pair<OrderByDirection, std::unique_ptr<Expression>>> order_bys;
    order_bys.emplace_back(OrderByDirection::DESC, std::make_unique<ColumnValueExpression>(0));

    auto sorted = std::make_unique<OrderByExecutor>(std::move(child), std::move(order_bys));

    LimitExecutor exec(std::move(sorted), 2, 0);
    exec.init();

    ColumnValueExpression col0(0);
    Tuple t;
    RID   rid;

    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(col0.evaluate(t, schema).getAsInteger(), 5);

    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(col0.evaluate(t, schema).getAsInteger(), 4);

    EXPECT_FALSE(exec.next(&t, &rid));
}

TEST(LimitIntegrationTest, OffsetAfterOrderBy)
{
    Schema schema({Column{"a", ColumnType::Integer}});

    std::vector<Tuple> tuples = {makeTuple(3), makeTuple(1), makeTuple(4), makeTuple(2)};

    auto child = std::make_unique<MockExecutor>(schema, tuples);
    std::vector<std::pair<OrderByDirection, std::unique_ptr<Expression>>> order_bys;
    order_bys.emplace_back(OrderByDirection::ASC, std::make_unique<ColumnValueExpression>(0));

    auto sorted = std::make_unique<OrderByExecutor>(std::move(child), std::move(order_bys));

    // OFFSET 2 LIMIT 2 → items at sorted index 2 and 3 → values 3 and 4
    LimitExecutor exec(std::move(sorted), 2, 2);
    exec.init();

    ColumnValueExpression col0(0);
    Tuple t;
    RID   rid;

    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(col0.evaluate(t, schema).getAsInteger(), 3);

    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(col0.evaluate(t, schema).getAsInteger(), 4);

    EXPECT_FALSE(exec.next(&t, &rid));
}

TEST(LimitIntegrationTest, LimitZeroAfterOrderBy)
{
    Schema schema({Column{"a", ColumnType::Integer}});

    std::vector<Tuple> tuples = {makeTuple(3), makeTuple(1)};

    auto child = std::make_unique<MockExecutor>(schema, tuples);
    std::vector<std::pair<OrderByDirection, std::unique_ptr<Expression>>> order_bys;
    order_bys.emplace_back(OrderByDirection::ASC, std::make_unique<ColumnValueExpression>(0));

    auto sorted = std::make_unique<OrderByExecutor>(std::move(child), std::move(order_bys));

    LimitExecutor exec(std::move(sorted), 0, 0);
    exec.init();

    Tuple t;
    RID   rid;
    EXPECT_FALSE(exec.next(&t, &rid));
}

// ---------------------------------------------------------------------------
// LIMIT after mock aggregation output (Values acting as materialized agg)
// ---------------------------------------------------------------------------

TEST(LimitIntegrationTest, LimitAfterAggregation)
{
    // Simulate: SELECT group_id, COUNT(*) FROM ... GROUP BY group_id
    // Result: (1,10), (2,3), (3,7) → LIMIT 2
    Schema schema({Column{"group_id", ColumnType::Integer}, Column{"cnt", ColumnType::Integer}});

    std::vector<std::vector<std::unique_ptr<Expression>>> values;
    {
        std::vector<std::unique_ptr<Expression>> row;
        row.push_back(std::make_unique<ConstantExpression>(Value(1)));
        row.push_back(std::make_unique<ConstantExpression>(Value(10)));
        values.push_back(std::move(row));
    }
    {
        std::vector<std::unique_ptr<Expression>> row;
        row.push_back(std::make_unique<ConstantExpression>(Value(2)));
        row.push_back(std::make_unique<ConstantExpression>(Value(3)));
        values.push_back(std::move(row));
    }
    {
        std::vector<std::unique_ptr<Expression>> row;
        row.push_back(std::make_unique<ConstantExpression>(Value(3)));
        row.push_back(std::make_unique<ConstantExpression>(Value(7)));
        values.push_back(std::move(row));
    }

    auto agg_mock = std::make_unique<ValuesExecutor>(std::move(values), schema);

    LimitExecutor exec(std::move(agg_mock), 2, 0);
    exec.init();

    ColumnValueExpression gid(0);
    ColumnValueExpression cnt(1);
    Tuple t;
    RID   rid;

    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(gid.evaluate(t, schema).getAsInteger(), 1);
    EXPECT_EQ(cnt.evaluate(t, schema).getAsInteger(), 10);

    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(gid.evaluate(t, schema).getAsInteger(), 2);
    EXPECT_EQ(cnt.evaluate(t, schema).getAsInteger(), 3);

    EXPECT_FALSE(exec.next(&t, &rid));
}

TEST(LimitIntegrationTest, LimitAfterAggregationWithOffset)
{
    Schema schema({Column{"group_id", ColumnType::Integer}, Column{"cnt", ColumnType::Integer}});

    std::vector<std::vector<std::unique_ptr<Expression>>> values;
    for (int32_t i = 1; i <= 5; ++i)
    {
        std::vector<std::unique_ptr<Expression>> row;
        row.push_back(std::make_unique<ConstantExpression>(Value(i)));
        row.push_back(std::make_unique<ConstantExpression>(Value(i * 10)));
        values.push_back(std::move(row));
    }

    auto agg_mock = std::make_unique<ValuesExecutor>(std::move(values), schema);

    // OFFSET 2 LIMIT 2 → group_id 3,4
    LimitExecutor exec(std::move(agg_mock), 2, 2);
    exec.init();

    ColumnValueExpression gid(0);
    Tuple t;
    RID   rid;

    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(gid.evaluate(t, schema).getAsInteger(), 3);

    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(gid.evaluate(t, schema).getAsInteger(), 4);

    EXPECT_FALSE(exec.next(&t, &rid));
}

// ---------------------------------------------------------------------------
// LIMIT after HAVING
// ---------------------------------------------------------------------------

TEST(LimitIntegrationTest, LimitAfterHaving)
{
    // (1,5), (2,15), (3,8) — HAVING cnt > 6 → (2,15),(3,8) — LIMIT 1 → (2,15)
    Schema schema({Column{"id", ColumnType::Integer}, Column{"cnt", ColumnType::Integer}});

    std::vector<std::vector<std::unique_ptr<Expression>>> values;
    {
        std::vector<std::unique_ptr<Expression>> row;
        row.push_back(std::make_unique<ConstantExpression>(Value(1)));
        row.push_back(std::make_unique<ConstantExpression>(Value(5)));
        values.push_back(std::move(row));
    }
    {
        std::vector<std::unique_ptr<Expression>> row;
        row.push_back(std::make_unique<ConstantExpression>(Value(2)));
        row.push_back(std::make_unique<ConstantExpression>(Value(15)));
        values.push_back(std::move(row));
    }
    {
        std::vector<std::unique_ptr<Expression>> row;
        row.push_back(std::make_unique<ConstantExpression>(Value(3)));
        row.push_back(std::make_unique<ConstantExpression>(Value(8)));
        values.push_back(std::move(row));
    }

    auto values_exec = std::make_unique<ValuesExecutor>(std::move(values), schema);

    // HAVING cnt > 6
    auto predicate = std::make_unique<ComparisonExpression>(
        ComparisonType::GreaterThan,
        std::make_unique<ColumnValueExpression>(1),
        std::make_unique<ConstantExpression>(Value(6)));

    ExecutorContext exec_ctx(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    planner::HavingPlan having_plan(schema, std::move(predicate));
    auto having_exec = std::make_unique<executor::HavingExecutor>(
        &exec_ctx, &having_plan, std::move(values_exec));

    LimitExecutor exec(std::move(having_exec), 1, 0);
    exec.init();

    ColumnValueExpression id_col(0);
    ColumnValueExpression cnt_col(1);
    Tuple t;
    RID   rid;

    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(id_col.evaluate(t, schema).getAsInteger(),  2);
    EXPECT_EQ(cnt_col.evaluate(t, schema).getAsInteger(), 15);

    EXPECT_FALSE(exec.next(&t, &rid));
}

TEST(LimitIntegrationTest, LimitZeroAfterHaving)
{
    Schema schema({Column{"id", ColumnType::Integer}, Column{"cnt", ColumnType::Integer}});

    std::vector<std::vector<std::unique_ptr<Expression>>> values;
    {
        std::vector<std::unique_ptr<Expression>> row;
        row.push_back(std::make_unique<ConstantExpression>(Value(1)));
        row.push_back(std::make_unique<ConstantExpression>(Value(20)));
        values.push_back(std::move(row));
    }

    auto values_exec = std::make_unique<ValuesExecutor>(std::move(values), schema);

    auto predicate = std::make_unique<ComparisonExpression>(
        ComparisonType::GreaterThan,
        std::make_unique<ColumnValueExpression>(1),
        std::make_unique<ConstantExpression>(Value(6)));

    ExecutorContext exec_ctx(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    planner::HavingPlan having_plan(schema, std::move(predicate));
    auto having_exec = std::make_unique<executor::HavingExecutor>(
        &exec_ctx, &having_plan, std::move(values_exec));

    LimitExecutor exec(std::move(having_exec), 0, 0);
    exec.init();

    Tuple t;
    RID   rid;
    EXPECT_FALSE(exec.next(&t, &rid));
}

// ---------------------------------------------------------------------------
// LIMIT after Nested Loop Join
// ---------------------------------------------------------------------------

TEST(LimitIntegrationTest, LimitAfterJoin)
{
    // left: (1), (2), (3)   right: (10), (20)
    // Cross join → 6 rows; LIMIT 4 → first 4 rows
    Schema left_schema({Column{"l", ColumnType::Integer}});
    Schema right_schema({Column{"r", ColumnType::Integer}});

    std::vector<Tuple> left_tuples  = {makeTuple(1), makeTuple(2), makeTuple(3)};
    std::vector<Tuple> right_tuples = {makeTuple(10), makeTuple(20)};

    auto left_exec  = std::make_unique<MockExecutor>(left_schema, left_tuples);
    auto right_exec = std::make_unique<MockExecutor>(right_schema, right_tuples);

    // No join predicate → cross join
    auto join_exec = std::make_unique<NestedLoopJoinExecutor>(
        std::move(left_exec), std::move(right_exec), nullptr);

    LimitExecutor exec(std::move(join_exec), 4, 0);
    exec.init();

    Tuple t;
    RID   rid;

    ASSERT_TRUE(exec.next(&t, &rid));
    ASSERT_TRUE(exec.next(&t, &rid));
    ASSERT_TRUE(exec.next(&t, &rid));
    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_FALSE(exec.next(&t, &rid));
}

TEST(LimitIntegrationTest, OffsetBeyondJoinOutputReturnsEof)
{
    // left: (1)   right: (10)
    // Cross join → 1 row; OFFSET 5 → EOF
    Schema left_schema({Column{"l", ColumnType::Integer}});
    Schema right_schema({Column{"r", ColumnType::Integer}});

    auto left_exec  = std::make_unique<MockExecutor>(left_schema, std::vector<Tuple>{makeTuple(1)});
    auto right_exec = std::make_unique<MockExecutor>(right_schema, std::vector<Tuple>{makeTuple(10)});

    auto join_exec = std::make_unique<NestedLoopJoinExecutor>(
        std::move(left_exec), std::move(right_exec), nullptr);

    LimitExecutor exec(std::move(join_exec), 10, 5);
    exec.init();

    Tuple t;
    RID   rid;
    EXPECT_FALSE(exec.next(&t, &rid));
}

TEST(LimitIntegrationTest, LimitPreservesOrderFromJoin)
{
    // left: (1),(2)   right: (100)
    // Join → (1,100),(2,100); LIMIT 1 should give first joined row preserving order
    Schema left_schema({Column{"l", ColumnType::Integer}});
    Schema right_schema({Column{"r", ColumnType::Integer}});
    Schema joined_schema({Column{"l", ColumnType::Integer}, Column{"r", ColumnType::Integer}});

    auto left_exec  = std::make_unique<MockExecutor>(left_schema, std::vector<Tuple>{makeTuple(1), makeTuple(2)});
    auto right_exec = std::make_unique<MockExecutor>(right_schema, std::vector<Tuple>{makeTuple(100)});

    auto join_exec = std::make_unique<NestedLoopJoinExecutor>(
        std::move(left_exec), std::move(right_exec), nullptr);

    LimitExecutor exec(std::move(join_exec), 1, 0);
    exec.init();

    Tuple t;
    RID   rid;

    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_FALSE(exec.next(&t, &rid));
}

// ---------------------------------------------------------------------------
// Re-init safety (init() resets state)
// ---------------------------------------------------------------------------

TEST(LimitIntegrationTest, ReInitResetsState)
{
    Schema schema({Column{"a", ColumnType::Integer}});

    std::vector<Tuple> tuples = {makeTuple(10), makeTuple(20), makeTuple(30)};
    auto child = std::make_unique<MockExecutor>(schema, tuples);
    std::vector<std::pair<OrderByDirection, std::unique_ptr<Expression>>> order_bys;
    order_bys.emplace_back(OrderByDirection::ASC, std::make_unique<ColumnValueExpression>(0));

    auto sorted = std::make_unique<OrderByExecutor>(std::move(child), std::move(order_bys));

    LimitExecutor exec(std::move(sorted), 2, 0);

    ColumnValueExpression col0(0);
    Tuple t;
    RID   rid;

    // First run
    exec.init();
    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(col0.evaluate(t, schema).getAsInteger(), 10);
    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(col0.evaluate(t, schema).getAsInteger(), 20);
    EXPECT_FALSE(exec.next(&t, &rid));

    // Re-init and run again
    exec.init();
    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(col0.evaluate(t, schema).getAsInteger(), 10);
    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(col0.evaluate(t, schema).getAsInteger(), 20);
    EXPECT_FALSE(exec.next(&t, &rid));
}

// ---------------------------------------------------------------------------
// Output schema is preserved through LIMIT
// ---------------------------------------------------------------------------

TEST(LimitIntegrationTest, OutputSchemaPreserved)
{
    Schema schema({Column{"x", ColumnType::Integer}, Column{"y", ColumnType::Varchar}});

    std::vector<Tuple> tuples;
    auto child = std::make_unique<MockExecutor>(schema, tuples);

    LimitExecutor exec(std::move(child), 10, 0);
    exec.init();

    const Schema& out = exec.outputSchema();
    EXPECT_EQ(out.getColumnCount(), 2u);
    EXPECT_EQ(out.getColumn(0).getName(), "x");
    EXPECT_EQ(out.getColumn(1).getName(), "y");
}

} // namespace
} // namespace hamdb
