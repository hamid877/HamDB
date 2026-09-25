#include "executor/limit_executor.hpp"
#include "executor/column_value_expression.hpp"
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

        class LimitExecutorTest : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                std::vector<Column> cols;
                cols.emplace_back("a", ColumnType::Integer);
                child_schema_ = std::make_unique<Schema>(std::move(cols));
            }

            std::unique_ptr<Schema> child_schema_;
        };

        Tuple buildTuple(int32_t a)
        {
            std::vector<std::byte> buf(100);
            Serializer ser(buf);
            (void)ser.writeInt32(a);
            return Tuple(std::span<const std::byte>(buf.data(), ser.position()));
        }

        TEST_F(LimitExecutorTest, BasicLimit)
        {
            std::vector<Tuple> tuples;
            for (int i = 0; i < 5; ++i)
                tuples.push_back(buildTuple(i));

            auto mock_child = std::make_unique<MockExecutor>(*child_schema_, tuples);

            LimitExecutor limit(std::move(mock_child), 2, 0);
            limit.init();

            Tuple tuple;
            RID rid;
            ASSERT_TRUE(limit.next(&tuple, &rid));
            EXPECT_EQ(rid.getSlotId(), 0);
            ASSERT_TRUE(limit.next(&tuple, &rid));
            EXPECT_EQ(rid.getSlotId(), 1);
            EXPECT_FALSE(limit.next(&tuple, &rid));
        }

        TEST_F(LimitExecutorTest, BasicOffset)
        {
            std::vector<Tuple> tuples;
            for (int i = 0; i < 5; ++i)
                tuples.push_back(buildTuple(i));

            auto mock_child = std::make_unique<MockExecutor>(*child_schema_, tuples);

            LimitExecutor limit(std::move(mock_child), 5, 2);
            limit.init();

            Tuple tuple;
            RID rid;
            ASSERT_TRUE(limit.next(&tuple, &rid));
            EXPECT_EQ(rid.getSlotId(), 2);
            ASSERT_TRUE(limit.next(&tuple, &rid));
            EXPECT_EQ(rid.getSlotId(), 3);
            ASSERT_TRUE(limit.next(&tuple, &rid));
            EXPECT_EQ(rid.getSlotId(), 4);
            EXPECT_FALSE(limit.next(&tuple, &rid));
        }

        TEST_F(LimitExecutorTest, LimitZero)
        {
            std::vector<Tuple> tuples;
            for (int i = 0; i < 5; ++i)
                tuples.push_back(buildTuple(i));

            auto mock_child = std::make_unique<MockExecutor>(*child_schema_, tuples);

            LimitExecutor limit(std::move(mock_child), 0, 0);
            limit.init();

            Tuple tuple;
            RID rid;
            EXPECT_FALSE(limit.next(&tuple, &rid));
        }

        TEST_F(LimitExecutorTest, OffsetGreaterThanTotal)
        {
            std::vector<Tuple> tuples;
            for (int i = 0; i < 5; ++i)
                tuples.push_back(buildTuple(i));

            auto mock_child = std::make_unique<MockExecutor>(*child_schema_, tuples);

            LimitExecutor limit(std::move(mock_child), 5, 10);
            limit.init();

            Tuple tuple;
            RID rid;
            EXPECT_FALSE(limit.next(&tuple, &rid));
        }

        TEST_F(LimitExecutorTest, LimitPlusOffset)
        {
            std::vector<Tuple> tuples;
            for (int i = 0; i < 5; ++i)
                tuples.push_back(buildTuple(i));

            auto mock_child = std::make_unique<MockExecutor>(*child_schema_, tuples);

            LimitExecutor limit(std::move(mock_child), 2, 2);
            limit.init();

            Tuple tuple;
            RID rid;
            ASSERT_TRUE(limit.next(&tuple, &rid));
            EXPECT_EQ(rid.getSlotId(), 2);
            ASSERT_TRUE(limit.next(&tuple, &rid));
            EXPECT_EQ(rid.getSlotId(), 3);
            EXPECT_FALSE(limit.next(&tuple, &rid));
        }

    } // namespace
} // namespace hamdb
