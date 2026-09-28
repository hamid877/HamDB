#include "buffer/buffer_pool_manager.hpp"
#include "catalog/catalog_manager.hpp"
#include "storage/disk_manager.hpp"
#include <filesystem>
#include <gtest/gtest.h>
#include <memory>

namespace hamdb
{

    class CatalogTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const ::testing::TestInfo* const test_info = ::testing::UnitTest::GetInstance()->current_test_info();
            test_db_ = std::string("test_catalog_") + test_info->name() + ".hamdb";

            if (std::filesystem::exists(test_db_))
            {
                std::filesystem::remove(test_db_);
            }

            dm_ = std::make_unique<DiskManager>(test_db_);
            ASSERT_EQ(dm_->createDatabase(), Status::Ok);
            ASSERT_EQ(dm_->openDatabase(), Status::Ok);

            bpm_ = std::make_unique<BufferPoolManager>(32, *dm_);
            catalog_ = std::make_unique<CatalogManager>(bpm_.get());
        }

        void TearDown() override
        {
            catalog_.reset();
            bpm_.reset();
            dm_.reset();

            if (std::filesystem::exists(test_db_))
            {
                std::filesystem::remove(test_db_);
            }
        }

        std::string test_db_;
        std::unique_ptr<DiskManager> dm_;
        std::unique_ptr<BufferPoolManager> bpm_;
        std::unique_ptr<CatalogManager> catalog_;
    };

    TEST_F(CatalogTest, CreateAndGetTable)
    {
        std::vector<Column> cols;
        cols.emplace_back("id", ColumnType::Integer);
        cols.emplace_back("name", ColumnType::Varchar);
        Schema schema(cols);

        TableInfo* info = nullptr;
        auto status = catalog_->createTable("users", schema, info);
        ASSERT_EQ(status, Status::Ok);
        ASSERT_NE(info, nullptr);
        EXPECT_EQ(info->getTableName(), "users");
        EXPECT_EQ(info->getSchema().getColumnCount(), 2);

        TableInfo* fetched_info = nullptr;
        status = catalog_->getTable("users", fetched_info);
        ASSERT_EQ(status, Status::Ok);
        EXPECT_EQ(fetched_info->getTableId(), info->getTableId());
        EXPECT_EQ(fetched_info->getHeapRootPage(), info->getHeapRootPage());
        EXPECT_EQ(fetched_info->getIndexRootPage(), info->getIndexRootPage());
    }

    TEST_F(CatalogTest, Persistence)
    {
        std::vector<Column> cols;
        cols.emplace_back("id", ColumnType::Integer);
        Schema schema(cols);

        TableInfo* info = nullptr;
        auto status = catalog_->createTable("table1", schema, info);
        ASSERT_EQ(status, Status::Ok);

        EXPECT_EQ(bpm_->flushAllPages(), Status::Ok);
        
        // Ensure dirty catalog pages are flushed by destroying/resetting
        catalog_.reset();
        bpm_.reset();
        dm_.reset();

        // Reopen database
        dm_ = std::make_unique<DiskManager>(test_db_);
        ASSERT_EQ(dm_->openDatabase(), Status::Ok);
        bpm_ = std::make_unique<BufferPoolManager>(32, *dm_);
        catalog_ = std::make_unique<CatalogManager>(bpm_.get());

        info = nullptr;
        status = catalog_->getTable("table1", info);
        ASSERT_EQ(status, Status::Ok);
        ASSERT_NE(info, nullptr);
        EXPECT_EQ(info->getTableName(), "table1");
        EXPECT_EQ(info->getSchema().getColumnCount(), 1);
        EXPECT_EQ(info->getSchema().getColumn(0).getName(), "id");
    }

    TEST_F(CatalogTest, DropTable)
    {
        std::vector<Column> cols;
        cols.emplace_back("id", ColumnType::Integer);
        Schema schema(cols);

        TableInfo* info = nullptr;
        auto status = catalog_->createTable("table1", schema, info);
        EXPECT_EQ(status, Status::Ok);

        status = catalog_->dropTable("table1");
        EXPECT_EQ(status, Status::Ok);

        status = catalog_->getTable("table1", info);
        EXPECT_EQ(status, Status::NotFound);
    }

} // namespace hamdb
