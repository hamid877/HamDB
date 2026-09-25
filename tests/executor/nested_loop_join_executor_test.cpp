#include "executor/nested_loop_join_executor.hpp"
#include "executor/constant_expression.hpp"
#include "executor/column_value_expression.hpp"
#include "executor/comparison_expression.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace hamdb {
namespace {

std::vector<std::byte> makePayload(std::string_view s) {
    std::vector<std::byte> out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<std::byte>(c));
    }
    return out;
}

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

class NestedLoopJoinExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::vector<Column> left_cols;
        left_cols.emplace_back("left_col", ColumnType::Integer);
        left_schema_ = std::make_unique<Schema>(std::move(left_cols));

        std::vector<Column> right_cols;
        right_cols.emplace_back("right_col", ColumnType::Integer);
        right_schema_ = std::make_unique<Schema>(std::move(right_cols));
    }

    std::unique_ptr<Schema> left_schema_;
    std::unique_ptr<Schema> right_schema_;
};

TEST_F(NestedLoopJoinExecutorTest, CrossJoinNoPredicate) {
    std::vector<Tuple> left_tuples;
    left_tuples.emplace_back(makePayload("A"));
    left_tuples.emplace_back(makePayload("B"));

    std::vector<Tuple> right_tuples;
    right_tuples.emplace_back(makePayload("1"));
    right_tuples.emplace_back(makePayload("2"));

    auto left_child = std::make_unique<MockExecutor>(*left_schema_, left_tuples);
    auto right_child = std::make_unique<MockExecutor>(*right_schema_, right_tuples);

    NestedLoopJoinExecutor join(std::move(left_child), std::move(right_child), nullptr);
    join.init();

    EXPECT_EQ(join.outputSchema().getColumnCount(), 2);
    EXPECT_EQ(join.outputSchema().getColumn(0).getName(), "left_col");
    EXPECT_EQ(join.outputSchema().getColumn(1).getName(), "right_col");

    Tuple tuple;
    RID rid;
    
    ASSERT_TRUE(join.next(&tuple, &rid));
    EXPECT_EQ(tuple.size(), 2); // "A1"
    
    ASSERT_TRUE(join.next(&tuple, &rid));
    EXPECT_EQ(tuple.size(), 2); // "A2"
    
    ASSERT_TRUE(join.next(&tuple, &rid));
    EXPECT_EQ(tuple.size(), 2); // "B1"
    
    ASSERT_TRUE(join.next(&tuple, &rid));
    EXPECT_EQ(tuple.size(), 2); // "B2"
    
    EXPECT_FALSE(join.next(&tuple, &rid));
}

TEST_F(NestedLoopJoinExecutorTest, InnerJoinWithPredicate) {
    std::vector<Tuple> left_tuples;
    left_tuples.emplace_back(makePayload("A"));
    left_tuples.emplace_back(makePayload("B"));

    std::vector<Tuple> right_tuples;
    right_tuples.emplace_back(makePayload("1"));
    right_tuples.emplace_back(makePayload("2"));

    auto left_child = std::make_unique<MockExecutor>(*left_schema_, left_tuples);
    auto right_child = std::make_unique<MockExecutor>(*right_schema_, right_tuples);

    auto false_predicate = std::make_unique<ConstantExpression>(Value(false));

    NestedLoopJoinExecutor join(std::move(left_child), std::move(right_child), std::move(false_predicate));
    join.init();

    Tuple tuple;
    RID rid;
    
    // Everything is filtered out
    EXPECT_FALSE(join.next(&tuple, &rid));
}

} // namespace
} // namespace hamdb
