#include "executor/projection_executor.hpp"
#include "executor/constant_expression.hpp"
#include "executor/column_value_expression.hpp"
#include "executor/arithmetic_expression.hpp"
#include "utils/serializer.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace hamdb {
namespace {

class MockExecutor : public AbstractExecutor {
public:
    MockExecutor(Schema schema, std::vector<Tuple> tuples)
        : schema_(std::move(schema)), tuples_(std::move(tuples)) {}

    void init() override {
        idx_ = 0;
    }

    bool next(Tuple* tuple, RID* rid) override {
        if (idx_ < tuples_.size()) {
            *tuple = tuples_[idx_];
            *rid = RID(0, idx_);
            idx_++;
            return true;
        }
        return false;
    }

    const Schema& outputSchema() const override {
        return schema_;
    }

private:
    Schema schema_;
    std::vector<Tuple> tuples_;
    size_t idx_{0};
};

class ProjectionExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::vector<Column> cols;
        cols.emplace_back("a", ColumnType::Integer);
        cols.emplace_back("b", ColumnType::Integer);
        child_schema_ = std::make_unique<Schema>(std::move(cols));
    }

    std::unique_ptr<Schema> child_schema_;
};

Tuple buildTuple(int32_t a, int32_t b) {
    std::vector<std::byte> buf(100);
    Serializer ser(buf);
    (void)ser.writeInt32(a);
    (void)ser.writeInt32(b);
    return Tuple(std::span<const std::byte>(buf.data(), ser.position()));
}

TEST_F(ProjectionExecutorTest, BasicProjection) {
    std::vector<Tuple> tuples;
    tuples.push_back(buildTuple(10, 20));
    tuples.push_back(buildTuple(30, 40));

    auto mock_child = std::make_unique<MockExecutor>(*child_schema_, tuples);

    // Expressions: [ a + b, 100 ]
    std::vector<std::unique_ptr<Expression>> exprs;
    auto col_a = std::make_unique<ColumnValueExpression>(0);
    auto col_b = std::make_unique<ColumnValueExpression>(1);
    auto a_plus_b = std::make_unique<ArithmeticExpression>(ArithmeticType::Add, std::move(col_a), std::move(col_b));
    exprs.push_back(std::move(a_plus_b));

    auto const_expr = std::make_unique<ConstantExpression>(Value(100));
    exprs.push_back(std::move(const_expr));

    std::vector<Column> out_cols;
    out_cols.emplace_back("a_plus_b", ColumnType::Integer);
    out_cols.emplace_back("const_100", ColumnType::Integer);
    Schema out_schema(std::move(out_cols));

    ProjectionExecutor proj(std::move(mock_child), std::move(exprs), out_schema);
    proj.init();

    Tuple tuple;
    RID rid;
    
    ASSERT_TRUE(proj.next(&tuple, &rid));
    ColumnValueExpression out_col0(0);
    ColumnValueExpression out_col1(1);
    EXPECT_EQ(out_col0.evaluate(tuple, out_schema).getAsInteger(), 30);
    EXPECT_EQ(out_col1.evaluate(tuple, out_schema).getAsInteger(), 100);
    EXPECT_EQ(rid.getSlotId(), 0);

    ASSERT_TRUE(proj.next(&tuple, &rid));
    EXPECT_EQ(out_col0.evaluate(tuple, out_schema).getAsInteger(), 70);
    EXPECT_EQ(out_col1.evaluate(tuple, out_schema).getAsInteger(), 100);
    EXPECT_EQ(rid.getSlotId(), 1);

    EXPECT_FALSE(proj.next(&tuple, &rid));
}

} // namespace
} // namespace hamdb
