#include "common/enums.hpp"
#include "storage/disk_manager.hpp"
#include <filesystem>
#include <gtest/gtest.h>

namespace hamdb
{

    // ── DiskManager construction ───────────────────────────────────────────────────
    //
    // NOTE: Full I/O tests will be added in Milestone 2.
    //       These tests only verify the scaffolding compiles and the object
    //       can be constructed.

    TEST(DiskManagerTest, ConstructionDoesNotThrow)
    {
        EXPECT_NO_THROW(
            { DiskManager dm(std::filesystem::temp_directory_path() / "hamdb_test.db"); });
    }

    TEST(DiskManagerTest, InitialPageCountIsZero)
    {
        DiskManager dm(std::filesystem::temp_directory_path() / "hamdb_test2.db");
        EXPECT_EQ(dm.pageCount(), 0u);
    }

    TEST(DiskManagerTest, FilePathMatchesConstructorArgument)
    {
        auto path = std::filesystem::temp_directory_path() / "hamdb_test3.db";
        DiskManager dm(path);
        EXPECT_EQ(dm.filePath(), path);
    }

    TEST(DiskManagerTest, ReadPageReturnsNotSupported)
    {
        DiskManager dm(std::filesystem::temp_directory_path() / "hamdb_test4.db");
        Page p;
        EXPECT_EQ(dm.readPage(0, p), Status::NotSupported);
    }

    TEST(DiskManagerTest, WritePageReturnsNotSupported)
    {
        DiskManager dm(std::filesystem::temp_directory_path() / "hamdb_test5.db");
        Page p;
        EXPECT_EQ(dm.writePage(0, p), Status::NotSupported);
    }

    TEST(DiskManagerTest, AllocatePageReturnsNotSupported)
    {
        DiskManager dm(std::filesystem::temp_directory_path() / "hamdb_test6.db");
        PageId id{};
        EXPECT_EQ(dm.allocatePage(id), Status::NotSupported);
    }

    TEST(DiskManagerTest, SyncReturnsNotSupported)
    {
        DiskManager dm(std::filesystem::temp_directory_path() / "hamdb_test7.db");
        EXPECT_EQ(dm.sync(), Status::NotSupported);
    }

} // namespace hamdb
