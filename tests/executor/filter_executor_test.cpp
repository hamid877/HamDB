#include "buffer/buffer_pool_manager.hpp"
#include "catalog/catalog_manager.hpp"
#include "executor/column_value_expression.hpp"
#include "executor/comparison_expression.hpp"
#include "executor/constant_expression.hpp"
#include "executor/executor_context.hpp"
#include "executor/filter_executor.hpp"
#include "executor/logical_expression.hpp"
#include "executor/seq_scan_executor.hpp"
#include "storage/disk_manager.hpp"
#include "transaction/lock_manager.hpp"
#include "transaction/mvcc_manager.hpp"
#include "transaction/transaction_manager.hpp"
#include "wal/log_manager.hpp"

#include <filesystem>
#include <gtest/gtest.h>
#include <vector>

namespace hamdb
{
    namespace
    {

        std::vector<std::byte> makePayload(std::string_view s)
        {
            std::vector<std::byte> out;
            out.reserve(s.size());
            for (char c : s)
            {
                out.push_back(static_cast<std::byte>(c));
            }
            return out;
        }

        // Since Schema requires type size for Varchar, let's just make it simple using Integer for
        // testing The Tuple constructor with vectors might be an issue. But we don't have Tuple
        // builder here. Let's create a Mock Executor to yield simple tuples instead of full SeqScan
        // if possible. But we can just use the provided SeqScanExecutor since we have it!

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

        class FilterExecutorTest : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                std::vector<Column> cols;
                cols.emplace_back("col1", ColumnType::Integer);
                cols.emplace_back("col2", ColumnType::Integer);
                schema_ = std::make_unique<Schema>(std::move(cols));
            }

            std::unique_ptr<Schema> schema_;
        };

        // We don't have a Tuple(std::vector<Value>, const Schema&) constructor maybe?
        // Let's check how ColumnValueExpression works or we can just use ConstantExpression to test
        // Filter logic alone without unpacking a Tuple!

        TEST_F(FilterExecutorTest, BasicFilter)
        {
            // We will just test with mock tuples, but the predicate evaluates to true/false
            // constantly
            std::vector<Tuple> tuples;
            tuples.emplace_back(makePayload("A"));
            tuples.emplace_back(makePayload("B"));
            tuples.emplace_back(makePayload("C"));

            auto mock_child = std::make_unique<MockExecutor>(*schema_, tuples);

            // Predicate: True
            auto predicate = std::make_unique<ConstantExpression>(Value(true));

            FilterExecutor filter(std::move(mock_child), std::move(predicate));
            filter.init();

            Tuple tuple;
            RID rid;
            ASSERT_TRUE(filter.next(&tuple, &rid));
            EXPECT_EQ(rid.getSlotId(), 0);
            ASSERT_TRUE(filter.next(&tuple, &rid));
            EXPECT_EQ(rid.getSlotId(), 1);
            ASSERT_TRUE(filter.next(&tuple, &rid));
            EXPECT_EQ(rid.getSlotId(), 2);
            EXPECT_FALSE(filter.next(&tuple, &rid));
        }

        TEST_F(FilterExecutorTest, FilterOutAll)
        {
            std::vector<Tuple> tuples;
            tuples.emplace_back(makePayload("A"));
            tuples.emplace_back(makePayload("B"));

            auto mock_child = std::make_unique<MockExecutor>(*schema_, tuples);
            auto predicate = std::make_unique<ConstantExpression>(Value(false));

            FilterExecutor filter(std::move(mock_child), std::move(predicate));
            filter.init();

            Tuple tuple;
            RID rid;
            EXPECT_FALSE(filter.next(&tuple, &rid));
        }

        TEST_F(FilterExecutorTest, LogicalExpressionTest)
        {
            std::vector<Tuple> tuples;
            tuples.emplace_back(makePayload("A")); // just a dummy tuple

            auto mock_child = std::make_unique<MockExecutor>(*schema_, tuples);

            // Predicate: True AND (NOT False) -> True
            auto true_expr = std::make_unique<ConstantExpression>(Value(true));
            auto false_expr = std::make_unique<ConstantExpression>(Value(false));
            auto not_false =
                std::make_unique<LogicalExpression>(LogicalType::Not, std::move(false_expr));
            auto and_expr = std::make_unique<LogicalExpression>(
                LogicalType::And, std::move(true_expr), std::move(not_false));

            FilterExecutor filter(std::move(mock_child), std::move(and_expr));
            filter.init();

            Tuple tuple;
            RID rid;
            EXPECT_TRUE(filter.next(&tuple, &rid));
            EXPECT_FALSE(filter.next(&tuple, &rid));
        }

    } // namespace
} // namespace hamdb
