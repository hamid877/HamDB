#include "executor/values_executor.hpp"
#include "executor/column_value_expression.hpp"
#include "executor/constant_expression.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace hamdb
{
    namespace
    {

        TEST(ValuesExecutorTest, BasicValues)
        {
            std::vector<Column> cols;
            cols.emplace_back("a", ColumnType::Integer);
            cols.emplace_back("b", ColumnType::Boolean);
            cols.emplace_back("c", ColumnType::Varchar);
            Schema out_schema(std::move(cols));

            std::vector<std::vector<std::unique_ptr<Expression>>> values;

            // Row 1: (1, true, "hello")
            std::vector<std::unique_ptr<Expression>> row1;
            row1.push_back(std::make_unique<ConstantExpression>(Value(1)));
            row1.push_back(std::make_unique<ConstantExpression>(Value(true)));
            row1.push_back(std::make_unique<ConstantExpression>(Value("hello")));
            values.push_back(std::move(row1));

            // Row 2: (2, false, "world")
            std::vector<std::unique_ptr<Expression>> row2;
            row2.push_back(std::make_unique<ConstantExpression>(Value(2)));
            row2.push_back(std::make_unique<ConstantExpression>(Value(false)));
            row2.push_back(std::make_unique<ConstantExpression>(Value("world")));
            values.push_back(std::move(row2));

            ValuesExecutor values_exec(std::move(values), out_schema);
            values_exec.init();

            Tuple tuple;
            RID rid;

            ColumnValueExpression col0(0);
            ColumnValueExpression col1(1);
            ColumnValueExpression col2(2);

            ASSERT_TRUE(values_exec.next(&tuple, &rid));
            EXPECT_EQ(col0.evaluate(tuple, out_schema).getAsInteger(), 1);
            EXPECT_EQ(col1.evaluate(tuple, out_schema).getAsBoolean(), true);
            EXPECT_EQ(col2.evaluate(tuple, out_schema).getAsVarchar(), "hello");

            ASSERT_TRUE(values_exec.next(&tuple, &rid));
            EXPECT_EQ(col0.evaluate(tuple, out_schema).getAsInteger(), 2);
            EXPECT_EQ(col1.evaluate(tuple, out_schema).getAsBoolean(), false);
            EXPECT_EQ(col2.evaluate(tuple, out_schema).getAsVarchar(), "world");

            EXPECT_FALSE(values_exec.next(&tuple, &rid));
        }

        TEST(ValuesExecutorTest, EmptyValues)
        {
            std::vector<Column> cols;
            cols.emplace_back("a", ColumnType::Integer);
            Schema out_schema(std::move(cols));

            std::vector<std::vector<std::unique_ptr<Expression>>> values;

            ValuesExecutor values_exec(std::move(values), out_schema);
            values_exec.init();

            Tuple tuple;
            RID rid;
            EXPECT_FALSE(values_exec.next(&tuple, &rid));
        }

    } // namespace
} // namespace hamdb
