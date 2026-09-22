#include "buffer/buffer_pool_manager.hpp"
#include "storage/disk_manager.hpp"
#include <filesystem>
#include <gtest/gtest.h>

namespace hamdb
{

    class BufferPoolManagerTest : public ::testing::Test
    {
    protected:
        std::filesystem::path db_path_ = std::filesystem::temp_directory_path() / "bpm_test.db";

        void SetUp() override
        {
            std::filesystem::remove(db_path_);
        }

        void TearDown() override
        {
            std::filesystem::remove(db_path_);
        }
    };

    TEST_F(BufferPoolManagerTest, FetchingSamePageTwiceReturnsSameFrame)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        
        BufferPoolManager bpm(5, dm);
        
        PageId page_id = 0;
        BufferFrame* frame1 = nullptr;
        BufferFrame* frame2 = nullptr;

        // Fetch first time
        ASSERT_EQ(bpm.fetchPage(page_id, frame1), Status::Ok);
        ASSERT_NE(frame1, nullptr);
        EXPECT_EQ(frame1->pinCount(), 1);

        // Fetch second time
        ASSERT_EQ(bpm.fetchPage(page_id, frame2), Status::Ok);
        ASSERT_NE(frame2, nullptr);
        EXPECT_EQ(frame2->pinCount(), 2);
        
        EXPECT_EQ(frame1, frame2);
        EXPECT_EQ(frame1->frameId(), frame2->frameId());
        
        EXPECT_EQ(bpm.unpinPage(page_id, false), Status::Ok);
        EXPECT_EQ(bpm.unpinPage(page_id, false), Status::Ok);
    }

    TEST_F(BufferPoolManagerTest, DirtyPagePersistsAfterFlushAndReopen)
    {
        {
            DiskManager dm(db_path_);
            ASSERT_EQ(dm.createDatabase(), Status::Ok);
            ASSERT_EQ(dm.openDatabase(), Status::Ok);
            
            BufferPoolManager bpm(5, dm);
            
            PageId page_id = kInvalidPageId;
            BufferFrame* frame = nullptr;
            ASSERT_EQ(bpm.newPage(page_id, frame), Status::Ok);
            
            // Modify page data (after header, e.g. offset 16)
            frame->page().data()[PageHeader::kSize] = std::byte{'A'};
            
            // Unpin as dirty
            EXPECT_EQ(bpm.unpinPage(page_id, true), Status::Ok);
            
            // Flush all
            EXPECT_EQ(bpm.flushAllPages(), Status::Ok);
        }
        
        // Reopen database
        {
            DiskManager dm(db_path_);
            ASSERT_EQ(dm.openDatabase(), Status::Ok);
            
            BufferPoolManager bpm(5, dm);
            
            PageId page_id = 1; // It was the first newly allocated page
            BufferFrame* frame = nullptr;
            ASSERT_EQ(bpm.fetchPage(page_id, frame), Status::Ok);
            
            // Verify data persisted
            EXPECT_EQ(frame->page().data()[PageHeader::kSize], std::byte{'A'});
            
            EXPECT_EQ(bpm.unpinPage(page_id, false), Status::Ok);
        }
    }

    TEST_F(BufferPoolManagerTest, UnpinBelowZeroFails)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        
        BufferPoolManager bpm(5, dm);
        
        PageId page_id = 0;
        BufferFrame* frame = nullptr;
        ASSERT_EQ(bpm.fetchPage(page_id, frame), Status::Ok);
        
        // Unpin down to zero
        EXPECT_EQ(bpm.unpinPage(page_id, false), Status::Ok);
        
        // Unpin again should fail
        EXPECT_EQ(bpm.unpinPage(page_id, false), Status::InvalidArg);
    }

    TEST_F(BufferPoolManagerTest, FlushInvalidPageReturnsNotFound)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        
        BufferPoolManager bpm(5, dm);
        
        // Flushing page that hasn't been fetched
        EXPECT_EQ(bpm.flushPage(123), Status::NotFound);
    }

    TEST_F(BufferPoolManagerTest, CleanPageFlushIsNoOp)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        
        BufferPoolManager bpm(5, dm);
        
        PageId page_id = 0;
        BufferFrame* frame = nullptr;
        ASSERT_EQ(bpm.fetchPage(page_id, frame), Status::Ok);
        
        // Clean page
        EXPECT_FALSE(frame->isDirty());
        
        // Unpin as clean
        EXPECT_EQ(bpm.unpinPage(page_id, false), Status::Ok);
        
        // Flush should be a no-op (Status::Ok)
        EXPECT_EQ(bpm.flushPage(page_id), Status::Ok);
    }

    TEST_F(BufferPoolManagerTest, PoolFullWithoutEviction)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        
        BufferPoolManager bpm(3, dm);
        
        BufferFrame* frame1 = nullptr;
        BufferFrame* frame2 = nullptr;
        BufferFrame* frame3 = nullptr;
        BufferFrame* frame4 = nullptr;
        
        PageId p1, p2, p3, p4;
        
        ASSERT_EQ(bpm.newPage(p1, frame1), Status::Ok);
        ASSERT_EQ(bpm.newPage(p2, frame2), Status::Ok);
        ASSERT_EQ(bpm.newPage(p3, frame3), Status::Ok);
        
        // Next allocation should fail because pool is full (size 3) and we don't evict yet
        EXPECT_EQ(bpm.newPage(p4, frame4), Status::BufferPoolFull);
        
        // Even if we unpin, because findFreeFrame only checks `!isValid()`, it will still fail
        EXPECT_EQ(bpm.unpinPage(p1, false), Status::Ok);
        EXPECT_EQ(bpm.newPage(p4, frame4), Status::BufferPoolFull);
    }

} // namespace hamdb
