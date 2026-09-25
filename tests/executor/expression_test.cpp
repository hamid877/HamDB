#include "executor/arithmetic_expression.hpp"
#include "executor/column_value_expression.hpp"
#include "executor/comparison_expression.hpp"
#include "executor/constant_expression.hpp"
#include "executor/value.hpp"
#include "utils/serializer.hpp"
#include <gtest/gtest.h>

namespace hamdb
{

    TEST(ExpressionTest, ValueBasic)
    {
        Value v1(42);
        Value v2(10);
        Value sum = v1.add(v2);
        EXPECT_EQ(sum.getAsInteger(), 52);

        Value v3(true);
        Value v4(false);
        EXPECT_TRUE(v3.getAsBoolean());
        EXPECT_FALSE(v4.getAsBoolean());
    }

    TEST(ExpressionTest, ConstantExpression)
    {
        ConstantExpression expr(Value(100));
        Tuple dummy;
        Schema schema;
        Value result = expr.evaluate(dummy, schema);
        EXPECT_EQ(result.getAsInteger(), 100);
    }

    TEST(ExpressionTest, ArithmeticExpression)
    {
        auto left = std::make_unique<ConstantExpression>(Value(10));
        auto right = std::make_unique<ConstantExpression>(Value(5));
        ArithmeticExpression add(ArithmeticType::Add, std::move(left), std::move(right));

        Tuple dummy;
        Schema schema;
        Value result = add.evaluate(dummy, schema);
        EXPECT_EQ(result.getAsInteger(), 15);
    }

    TEST(ExpressionTest, ComparisonExpression)
    {
        auto left = std::make_unique<ConstantExpression>(Value(10));
        auto right = std::make_unique<ConstantExpression>(Value(5));
        ComparisonExpression comp(ComparisonType::GreaterThan, std::move(left), std::move(right));

        Tuple dummy;
        Schema schema;
        Value result = comp.evaluate(dummy, schema);
        EXPECT_TRUE(result.getAsBoolean());
    }

    TEST(ExpressionTest, ColumnValueExpression)
    {
        Schema schema({Column("id", ColumnType::Integer), Column("name", ColumnType::Varchar),
                       Column("is_active", ColumnType::Boolean)});

        std::vector<std::byte> buffer(1024);
        Serializer serializer(buffer);
        EXPECT_EQ(serializer.writeInt32(42), Status::Ok);
        EXPECT_EQ(serializer.writeString("HamDB"), Status::Ok);
        EXPECT_EQ(serializer.writeBool(true), Status::Ok);

        Tuple tuple(std::span<const std::byte>(buffer.data(), serializer.position()));

        ColumnValueExpression col1(0);
        ColumnValueExpression col2(1);
        ColumnValueExpression col3(2);

        Value v1 = col1.evaluate(tuple, schema);
        Value v2 = col2.evaluate(tuple, schema);
        Value v3 = col3.evaluate(tuple, schema);

        EXPECT_EQ(v1.getAsInteger(), 42);
        EXPECT_EQ(v2.getAsVarchar(), "HamDB");
        EXPECT_TRUE(v3.getAsBoolean());
    }

} // namespace hamdb
