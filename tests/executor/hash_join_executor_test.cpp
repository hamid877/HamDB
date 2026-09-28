#include "executor/column_value_expression.hpp"
#include "executor/comparison_expression.hpp"
#include "executor/constant_expression.hpp"
#include "executor/hash_join_executor.hpp"
#include "catalog/catalog_manager.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include "storage/disk_manager.hpp"

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

        class HashJoinExecutorTest : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                std::string test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
                test_db_ = std::filesystem::temp_directory_path() / (std::string("test_hash_join_executor_") + test_name + ".hamdb");
                if (std::filesystem::exists(test_db_))
                {
                    std::filesystem::remove(test_db_);
                }

                disk_manager_ = std::make_unique<DiskManager>(test_db_.string());
                [[maybe_unused]] auto st1 = disk_manager_->createDatabase();
                [[maybe_unused]] auto st2 = disk_manager_->openDatabase();

                bpm_ = std::make_unique<BufferPoolManager>(10, *disk_manager_);
                catalog_ = std::make_unique<CatalogManager>(bpm_.get());

                std::vector<Column> left_cols;
                left_cols.emplace_back("left_col", ColumnType::Integer);
                left_schema_ = std::make_unique<Schema>(std::move(left_cols));

                std::vector<Column> right_cols;
                right_cols.emplace_back("right_col", ColumnType::Integer);
                right_schema_ = std::make_unique<Schema>(std::move(right_cols));
            }

            void TearDown() override
            {
                join_.reset();
                catalog_.reset();
                bpm_.reset();
                disk_manager_.reset();
                if (std::filesystem::exists(test_db_))
                {
                    std::filesystem::remove(test_db_);
                }
            }

            std::filesystem::path test_db_;
            std::unique_ptr<DiskManager> disk_manager_;
            std::unique_ptr<BufferPoolManager> bpm_;
            std::unique_ptr<CatalogManager> catalog_;

            std::unique_ptr<Schema> left_schema_;
            std::unique_ptr<Schema> right_schema_;
            std::unique_ptr<HashJoinExecutor> join_;
        };

        TEST_F(HashJoinExecutorTest, InnerHashJoin)
        {
            // Left tuples: 1, 2, 3, 2
            std::vector<Tuple> left_tuples;
            left_tuples.emplace_back(makePayload(std::string(reinterpret_cast<const char*>(new int32_t(1)), 4)));
            left_tuples.emplace_back(makePayload(std::string(reinterpret_cast<const char*>(new int32_t(2)), 4)));
            left_tuples.emplace_back(makePayload(std::string(reinterpret_cast<const char*>(new int32_t(3)), 4)));
            left_tuples.emplace_back(makePayload(std::string(reinterpret_cast<const char*>(new int32_t(2)), 4)));

            // Right tuples: 2, 4, 2, 1
            std::vector<Tuple> right_tuples;
            right_tuples.emplace_back(makePayload(std::string(reinterpret_cast<const char*>(new int32_t(2)), 4)));
            right_tuples.emplace_back(makePayload(std::string(reinterpret_cast<const char*>(new int32_t(4)), 4)));
            right_tuples.emplace_back(makePayload(std::string(reinterpret_cast<const char*>(new int32_t(2)), 4)));
            right_tuples.emplace_back(makePayload(std::string(reinterpret_cast<const char*>(new int32_t(1)), 4)));

            auto left_child = std::make_unique<MockExecutor>(*left_schema_, left_tuples);
            auto right_child = std::make_unique<MockExecutor>(*right_schema_, right_tuples);

            auto left_key = std::make_unique<ColumnValueExpression>(0); // left_col
            auto right_key = std::make_unique<ColumnValueExpression>(0); // right_col

            join_ = std::make_unique<HashJoinExecutor>(std::move(left_child), std::move(right_child),
                                                       std::move(left_key), std::move(right_key));
            join_->init();

            EXPECT_EQ(join_->outputSchema().getColumnCount(), 2);
            EXPECT_EQ(join_->outputSchema().getColumn(0).getName(), "left_col");
            EXPECT_EQ(join_->outputSchema().getColumn(1).getName(), "right_col");

            Tuple tuple;
            RID rid;

            int match_count = 0;
            while (join_->next(&tuple, &rid)) {
                match_count++;
            }
            
            // matches:
            // L:1, R:1 (1 match)
            // L:2 (first), R:2 (first)
            // L:2 (first), R:2 (second)
            // L:2 (second), R:2 (first)
            // L:2 (second), R:2 (second)
            // Total matches: 5
            EXPECT_EQ(match_count, 5);
        }

    } // namespace
} // namespace hamdb
