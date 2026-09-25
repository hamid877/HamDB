#include "executor/column_value_expression.hpp"
#include "executor/constant_expression.hpp"
#include "executor/sort_executor.hpp"
#include "utils/serializer.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace hamdb
{
    namespace
    {

        class MockExecutor : public AbstractExecutor
        {
        public:
            MockExecutor(Schema schema, std::vector<Tuple> tuples)
                : schema_(std::move(schema)), tuples_(std::move(tuples))
            {
            }

            void init() override
            {
                idx_ = 0;
            }

            bool next(Tuple* tuple, RID* rid) override
            {
                if (idx_ < tuples_.size())
                {
                    *tuple = tuples_[idx_];
                    *rid = RID(0, idx_);
                    idx_++;
                    return true;
                }
                return false;
            }

            const Schema& outputSchema() const override
            {
                return schema_;
            }

        private:
            Schema schema_;
            std::vector<Tuple> tuples_;
            size_t idx_{0};
        };

        class SortExecutorTest : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                std::vector<Column> cols;
                cols.emplace_back("col_a", ColumnType::Integer);
                cols.emplace_back("col_b", ColumnType::Integer);
                schema_ = std::make_unique<Schema>(std::move(cols));
            }

            std::unique_ptr<Schema> schema_;
        };

        Tuple buildTuple(int32_t a, int32_t b)
        {
            std::vector<std::byte> buf(100);
            Serializer ser(buf);
            (void)ser.writeInt32(a);
            (void)ser.writeInt32(b);
            return Tuple(std::span<const std::byte>(buf.data(), ser.position()));
        }

        TEST_F(SortExecutorTest, BasicSortAsc)
        {
            std::vector<Tuple> tuples;
            tuples.push_back(buildTuple(3, 10));
            tuples.push_back(buildTuple(1, 20));
            tuples.push_back(buildTuple(2, 50));

            auto mock_child = std::make_unique<MockExecutor>(*schema_, tuples);

            std::vector<std::pair<OrderByType, std::unique_ptr<Expression>>> order_bys;
            order_bys.emplace_back(OrderByType::ASC, std::make_unique<ColumnValueExpression>(0));

            SortExecutor sort_exec(std::move(mock_child), std::move(order_bys));
            sort_exec.init();

            Tuple tuple;
            RID rid;
            ColumnValueExpression out_col0(0);

            ASSERT_TRUE(sort_exec.next(&tuple, &rid));
            EXPECT_EQ(out_col0.evaluate(tuple, *schema_).getAsInteger(), 1);

            ASSERT_TRUE(sort_exec.next(&tuple, &rid));
            EXPECT_EQ(out_col0.evaluate(tuple, *schema_).getAsInteger(), 2);

            ASSERT_TRUE(sort_exec.next(&tuple, &rid));
            EXPECT_EQ(out_col0.evaluate(tuple, *schema_).getAsInteger(), 3);

            EXPECT_FALSE(sort_exec.next(&tuple, &rid));
        }

        TEST_F(SortExecutorTest, SortMultipleKeys)
        {
            std::vector<Tuple> tuples;
            tuples.push_back(buildTuple(1, 20));
            tuples.push_back(buildTuple(2, 10));
            tuples.push_back(buildTuple(1, 10));
            tuples.push_back(buildTuple(2, 20));

            auto mock_child = std::make_unique<MockExecutor>(*schema_, tuples);

            std::vector<std::pair<OrderByType, std::unique_ptr<Expression>>> order_bys;
            // ORDER BY col_a ASC, col_b DESC
            order_bys.emplace_back(OrderByType::ASC, std::make_unique<ColumnValueExpression>(0));
            order_bys.emplace_back(OrderByType::DESC, std::make_unique<ColumnValueExpression>(1));

            SortExecutor sort_exec(std::move(mock_child), std::move(order_bys));
            sort_exec.init();

            Tuple tuple;
            RID rid;
            ColumnValueExpression out_col0(0);
            ColumnValueExpression out_col1(1);

            ASSERT_TRUE(sort_exec.next(&tuple, &rid));
            EXPECT_EQ(out_col0.evaluate(tuple, *schema_).getAsInteger(), 1);
            EXPECT_EQ(out_col1.evaluate(tuple, *schema_).getAsInteger(), 20);

            ASSERT_TRUE(sort_exec.next(&tuple, &rid));
            EXPECT_EQ(out_col0.evaluate(tuple, *schema_).getAsInteger(), 1);
            EXPECT_EQ(out_col1.evaluate(tuple, *schema_).getAsInteger(), 10);

            ASSERT_TRUE(sort_exec.next(&tuple, &rid));
            EXPECT_EQ(out_col0.evaluate(tuple, *schema_).getAsInteger(), 2);
            EXPECT_EQ(out_col1.evaluate(tuple, *schema_).getAsInteger(), 20);

            ASSERT_TRUE(sort_exec.next(&tuple, &rid));
            EXPECT_EQ(out_col0.evaluate(tuple, *schema_).getAsInteger(), 2);
            EXPECT_EQ(out_col1.evaluate(tuple, *schema_).getAsInteger(), 10);

            EXPECT_FALSE(sort_exec.next(&tuple, &rid));
        }

    } // namespace
} // namespace hamdb
