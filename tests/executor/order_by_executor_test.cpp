#include <gtest/gtest.h>
#include "executor/order_by_executor.hpp"
#include "executor/column_value_expression.hpp"
#include "catalog/schema.hpp"
#include "utils/serializer.hpp"
#include <memory>
#include <vector>
#include <span>

namespace hamdb::test {

class MockExecutor : public AbstractExecutor {
public:
    MockExecutor(Schema schema, std::vector<Tuple> tuples)
        : schema_(std::move(schema)), tuples_(std::move(tuples)), idx_(0) {}
        
    void init() override { idx_ = 0; }
    bool next(Tuple* tuple, RID* rid) override {
        if (idx_ < tuples_.size()) {
            *tuple = tuples_[idx_];
            *rid = RID(0, idx_);
            idx_++;
            return true;
        }
        return false;
    }
    const Schema& outputSchema() const override { return schema_; }
private:
    Schema schema_;
    std::vector<Tuple> tuples_;
    size_t idx_;
};

Tuple buildTuple(int32_t a, int32_t b) {
    std::vector<std::byte> buf(100);
    Serializer ser(buf);
    (void)ser.writeInt32(a);
    (void)ser.writeInt32(b);
    return Tuple(std::span<const std::byte>(buf.data(), ser.position()));
}

TEST(OrderByExecutorTest, SortAscending) {
    Schema schema({Column{"a", ColumnType::Integer}, Column{"b", ColumnType::Integer}});
    std::vector<Tuple> tuples = {
        buildTuple(3, 10),
        buildTuple(1, 20),
        buildTuple(2, 30)
    };
    
    auto child = std::make_unique<MockExecutor>(schema, tuples);
    std::vector<std::pair<OrderByDirection, std::unique_ptr<Expression>>> order_bys;
    order_bys.emplace_back(OrderByDirection::ASC, std::make_unique<ColumnValueExpression>(0));
    
    OrderByExecutor exec(std::move(child), std::move(order_bys));
    exec.init();
    
    Tuple t;
    RID rid;
    ColumnValueExpression out_col0(0);
    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(out_col0.evaluate(t, schema).getAsInteger(), 1);
    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(out_col0.evaluate(t, schema).getAsInteger(), 2);
    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(out_col0.evaluate(t, schema).getAsInteger(), 3);
    ASSERT_FALSE(exec.next(&t, &rid));
}

TEST(OrderByExecutorTest, SortDescending) {
    Schema schema({Column{"a", ColumnType::Integer}, Column{"b", ColumnType::Integer}});
    std::vector<Tuple> tuples = {
        buildTuple(1, 10),
        buildTuple(3, 20),
        buildTuple(2, 30)
    };
    
    auto child = std::make_unique<MockExecutor>(schema, tuples);
    std::vector<std::pair<OrderByDirection, std::unique_ptr<Expression>>> order_bys;
    order_bys.emplace_back(OrderByDirection::DESC, std::make_unique<ColumnValueExpression>(0));
    
    OrderByExecutor exec(std::move(child), std::move(order_bys));
    exec.init();
    
    Tuple t;
    RID rid;
    ColumnValueExpression out_col0(0);
    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(out_col0.evaluate(t, schema).getAsInteger(), 3);
    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(out_col0.evaluate(t, schema).getAsInteger(), 2);
    ASSERT_TRUE(exec.next(&t, &rid));
    EXPECT_EQ(out_col0.evaluate(t, schema).getAsInteger(), 1);
    ASSERT_FALSE(exec.next(&t, &rid));
}

} // namespace hamdb::test
