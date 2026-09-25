#include "common/enums.hpp"
#include "storage/database_metadata.hpp"
#include "storage/disk_manager.hpp"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace hamdb
{

    // ── Helpers ───────────────────────────────────────────────────────────────

    /// RAII wrapper that deletes a file on scope exit (even on test failure).
    class TempFile
    {
    public:
        explicit TempFile(std::filesystem::path p) : path_(std::move(p))
        {
            std::filesystem::remove(path_); // ensure clean slate
        }
        ~TempFile()
        {
            std::filesystem::remove(path_);
        }
        [[nodiscard]] const std::filesystem::path& path() const
        {
            return path_;
        }

    private:
        std::filesystem::path path_;
    };

    std::filesystem::path tmpPath(const std::string& prefix)
    {
        auto dir = std::filesystem::temp_directory_path();

        auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

        return dir / (prefix + "-" + unique + ".hamdb");
    }

    // ── Construction (existing, kept as-is) ───────────────────────────────────

    TEST(DiskManagerTest, ConstructionDoesNotThrow)
    {
        EXPECT_NO_THROW({ DiskManager dm(tmpPath("hamdb_test.db")); });
    }

    TEST(DiskManagerTest, InitialPageCountIsZero)
    {
        DiskManager dm(tmpPath("hamdb_test2.db"));
        EXPECT_EQ(dm.pageCount(), 0u);
    }

    TEST(DiskManagerTest, FilePathMatchesConstructorArgument)
    {
        auto path = tmpPath("hamdb_test3.db");
        DiskManager dm(path);
        EXPECT_EQ(dm.filePath(), path);
    }

    TEST(DiskManagerTest, ReadPageReturnsIoErrorWhenClosed)
    {
        DiskManager dm(tmpPath("hamdb_test4.db"));
        Page p;
        EXPECT_EQ(dm.readPage(0, p), Status::IoError);
    }

    TEST(DiskManagerTest, WritePageReturnsIoErrorWhenClosed)
    {
        DiskManager dm(tmpPath("hamdb_test5.db"));
        Page p;
        EXPECT_EQ(dm.writePage(0, p), Status::IoError);
    }

    TEST(DiskManagerTest, AllocatePageReturnsIoErrorWhenClosed)
    {
        DiskManager dm(tmpPath("hamdb_test6.db"));
        PageId id{};
        EXPECT_EQ(dm.allocatePage(id), Status::IoError);
    }

    TEST(DiskManagerTest, SyncReturnsOkWhenClosed)
    {
        DiskManager dm(tmpPath("hamdb_test7.db"));
        EXPECT_EQ(dm.sync(), Status::Ok);
    }

    // ── createDatabase ────────────────────────────────────────────────────────

    TEST(DiskManagerCreateTest, CreateDatabaseReturnsOk)
    {
        TempFile tmp(tmpPath("create_ok.hamdb"));
        DiskManager dm(tmp.path());
        EXPECT_EQ(dm.createDatabase(), Status::Ok);
    }

    TEST(DiskManagerCreateTest, DuplicateCreateReturnsAlreadyExists)
    {
        TempFile tmp(tmpPath("create_dup.hamdb"));
        DiskManager dm(tmp.path());
        ASSERT_EQ(dm.createDatabase(), Status::Ok);

        // Second call must fail
        DiskManager dm2(tmp.path());
        EXPECT_EQ(dm2.createDatabase(), Status::AlreadyExists);
    }

    TEST(DiskManagerCreateTest, FileSizeIsExactlyOnePage)
    {
        TempFile tmp(tmpPath("create_size.hamdb"));
        DiskManager dm(tmp.path());
        ASSERT_EQ(dm.createDatabase(), Status::Ok);

        const auto size = std::filesystem::file_size(tmp.path());
        EXPECT_EQ(size, kPageSize);
    }

    TEST(DiskManagerCreateTest, MagicNumberIsHAMDB001)
    {
        TempFile tmp(tmpPath("create_magic.hamdb"));
        DiskManager dm(tmp.path());
        ASSERT_EQ(dm.createDatabase(), Status::Ok);

        // Read first 8 bytes of the file
        std::ifstream f(tmp.path(), std::ios::binary);
        ASSERT_TRUE(f.is_open());
        std::array<char, 8> magic{};
        f.read(magic.data(), 8);
        ASSERT_EQ(f.gcount(), 8);

        const std::string_view written(magic.data(), 8);
        EXPECT_EQ(written, kDbMagic);
    }

    TEST(DiskManagerCreateTest, VersionFieldIsOne)
    {
        TempFile tmp(tmpPath("create_version.hamdb"));
        DiskManager dm(tmp.path());
        ASSERT_EQ(dm.createDatabase(), Status::Ok);

        // Read and deserialise the metadata block
        std::ifstream f(tmp.path(), std::ios::binary);
        ASSERT_TRUE(f.is_open());
        std::array<std::uint8_t, DatabaseMetadata::kSize> buf{};
        f.read(reinterpret_cast<char*>(buf.data()),
               static_cast<std::streamsize>(DatabaseMetadata::kSize));
        ASSERT_EQ(f.gcount(), static_cast<std::streamsize>(DatabaseMetadata::kSize));

        DatabaseMetadata meta;
        meta.deserialize(buf);
        EXPECT_EQ(meta.version, static_cast<std::uint16_t>(kFormatVersion));
    }

    TEST(DiskManagerCreateTest, TimestampIsNonZero)
    {
        TempFile tmp(tmpPath("create_ts.hamdb"));
        DiskManager dm(tmp.path());
        ASSERT_EQ(dm.createDatabase(), Status::Ok);

        std::ifstream f(tmp.path(), std::ios::binary);
        ASSERT_TRUE(f.is_open());
        std::array<std::uint8_t, DatabaseMetadata::kSize> buf{};
        f.read(reinterpret_cast<char*>(buf.data()),
               static_cast<std::streamsize>(DatabaseMetadata::kSize));

        DatabaseMetadata meta;
        meta.deserialize(buf);
        EXPECT_GT(meta.created_at, 0u);
    }

    // ── openDatabase ─────────────────────────────────────────────────────────

    TEST(DiskManagerOpenTest, OpenExistingDatabaseReturnsOk)
    {
        TempFile tmp(tmpPath("open_ok.hamdb"));
        {
            DiskManager dm(tmp.path());
            ASSERT_EQ(dm.createDatabase(), Status::Ok);
        }
        DiskManager dm(tmp.path());
        EXPECT_EQ(dm.openDatabase(), Status::Ok);
        EXPECT_TRUE(dm.isOpen());
    }

    TEST(DiskManagerOpenTest, OpenNonExistentFileReturnsNotFound)
    {
        auto path = tmpPath("open_missing_xyz.hamdb");
        std::filesystem::remove(path); // ensure it really doesn't exist
        DiskManager dm(path);
        EXPECT_EQ(dm.openDatabase(), Status::NotFound);
    }

    TEST(DiskManagerOpenTest, OpenSetsCorrectPageCount)
    {
        TempFile tmp(tmpPath("open_pc.hamdb"));
        {
            DiskManager dm(tmp.path());
            ASSERT_EQ(dm.createDatabase(), Status::Ok);
        }
        DiskManager dm(tmp.path());
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        EXPECT_EQ(dm.pageCount(), 1u);
    }

    // ── closeDatabase ─────────────────────────────────────────────────────────

    TEST(DiskManagerCloseTest, CloseAfterOpenReturnsOk)
    {
        TempFile tmp(tmpPath("close_ok.hamdb"));
        DiskManager dm(tmp.path());
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        EXPECT_EQ(dm.closeDatabase(), Status::Ok);
        EXPECT_FALSE(dm.isOpen());
    }

    TEST(DiskManagerCloseTest, CloseWhenAlreadyClosedIsNoOp)
    {
        DiskManager dm(tmpPath("close_noop.hamdb"));
        EXPECT_EQ(dm.closeDatabase(), Status::Ok); // not open — must not crash
    }

} // namespace hamdb
