#include "buffer/buffer_pool_manager.hpp"
#include "catalog/catalog_manager.hpp"
#include "storage/disk_manager.hpp"
#include <filesystem>
#include <gtest/gtest.h>

namespace hamdb
{

    class CatalogTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            test_db_ = "test_catalog.hamdb";
            if (std::filesystem::exists(test_db_))
            {
                std::filesystem::remove(test_db_);
            }
        }

        void TearDown() override
        {
            if (std::filesystem::exists(test_db_))
            {
                std::filesystem::remove(test_db_);
            }
        }

        std::string test_db_;
    };

    TEST_F(CatalogTest, CreateAndGetTable)
    {
        DiskManager dm(test_db_);
        auto st = dm.createDatabase();
        ASSERT_EQ(st, Status::Ok);
        st = dm.openDatabase();
        ASSERT_EQ(st, Status::Ok);
        BufferPoolManager bpm(10, dm);
        CatalogManager catalog(&bpm);

        std::vector<Column> cols;
        cols.emplace_back("id", ColumnType::Integer);
        cols.emplace_back("name", ColumnType::Varchar);
        Schema schema(cols);

        TableInfo* info = nullptr;
        auto status = catalog.createTable("users", schema, info);
        ASSERT_EQ(status, Status::Ok);
        ASSERT_NE(info, nullptr);
        EXPECT_EQ(info->getTableName(), "users");
        EXPECT_EQ(info->getSchema().getColumnCount(), 2);

        TableInfo* fetched_info = nullptr;
        status = catalog.getTable("users", fetched_info);
        ASSERT_EQ(status, Status::Ok);
        EXPECT_EQ(fetched_info->getTableId(), info->getTableId());
        EXPECT_EQ(fetched_info->getHeapRootPage(), info->getHeapRootPage());
        EXPECT_EQ(fetched_info->getIndexRootPage(), info->getIndexRootPage());
    }

    TEST_F(CatalogTest, Persistence)
    {
        {
            DiskManager dm(test_db_);
            auto st = dm.createDatabase();
            ASSERT_EQ(st, Status::Ok);
            st = dm.openDatabase();
            ASSERT_EQ(st, Status::Ok);
            BufferPoolManager bpm(10, dm);
            CatalogManager catalog(&bpm);

            std::vector<Column> cols;
            cols.emplace_back("id", ColumnType::Integer);
            Schema schema(cols);

            TableInfo* info = nullptr;
            auto status = catalog.createTable("table1", schema, info);
            ASSERT_EQ(status, Status::Ok);

            EXPECT_EQ(bpm.flushAllPages(), Status::Ok);
        }

        {
            DiskManager dm(test_db_);
            auto st = dm.openDatabase();
            ASSERT_EQ(st, Status::Ok);
            BufferPoolManager bpm(10, dm);
            CatalogManager catalog(&bpm);

            TableInfo* info = nullptr;
            auto status = catalog.getTable("table1", info);
            ASSERT_EQ(status, Status::Ok);
            ASSERT_NE(info, nullptr);
            EXPECT_EQ(info->getTableName(), "table1");
            EXPECT_EQ(info->getSchema().getColumnCount(), 1);
            EXPECT_EQ(info->getSchema().getColumn(0).getName(), "id");
        }
    }

    TEST_F(CatalogTest, DropTable)
    {
        DiskManager dm(test_db_);
        auto st = dm.createDatabase();
        ASSERT_EQ(st, Status::Ok);
        st = dm.openDatabase();
        ASSERT_EQ(st, Status::Ok);
        BufferPoolManager bpm(10, dm);
        CatalogManager catalog(&bpm);

        std::vector<Column> cols;
        cols.emplace_back("id", ColumnType::Integer);
        Schema schema(cols);

        TableInfo* info = nullptr;
        auto status = catalog.createTable("table1", schema, info);
        EXPECT_EQ(status, Status::Ok);

        status = catalog.dropTable("table1");
        EXPECT_EQ(status, Status::Ok);

        status = catalog.getTable("table1", info);
        EXPECT_EQ(status, Status::NotFound);
    }

} // namespace hamdb
