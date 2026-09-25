#include "buffer/buffer_pool_manager.hpp"
#include "buffer/page_guard.hpp"
#include "storage/disk_manager.hpp"
#include "storage/page_header.hpp"
#include <filesystem>
#include <gtest/gtest.h>

namespace hamdb
{

    // =========================================================================
    // Test fixture
    // =========================================================================

    class PageGuardTest : public ::testing::Test
    {
    protected:
        std::filesystem::path db_path_ =
            std::filesystem::temp_directory_path() / "page_guard_test.db";

        void SetUp() override
        {
            std::filesystem::remove(db_path_);
        }

        void TearDown() override
        {
            std::filesystem::remove(db_path_);
        }

        /// Helper: create + open a DiskManager backed by db_path_.
        DiskManager makeDiskManager()
        {
            DiskManager dm(db_path_);
            if (!std::filesystem::exists(db_path_))
            {
                (void)dm.createDatabase();
            }
            (void)dm.openDatabase();
            return dm;
        }
    };

    // =========================================================================
    // BasicPageGuard tests
    // =========================================================================

    TEST_F(PageGuardTest, BasicGuardDefaultIsInvalid)
    {
        BasicPageGuard guard;
        EXPECT_FALSE(guard.isValid());
        EXPECT_EQ(guard.pageId(), kInvalidPageId);
    }

    TEST_F(PageGuardTest, BasicGuardAutoUnpinsOnDestruction)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        PageId page_id = kInvalidPageId;
        BufferFrame* frame = nullptr;
        ASSERT_EQ(bpm.newPage(page_id, frame), Status::Ok);
        ASSERT_NE(frame, nullptr);
        EXPECT_EQ(frame->pinCount(), 1);

        {
            BasicPageGuard guard(&bpm, frame);
            EXPECT_TRUE(guard.isValid());
            EXPECT_EQ(guard.pageId(), page_id);
            // pin count stays at 1 — the guard did NOT re-pin
            EXPECT_EQ(frame->pinCount(), 1);
        } // guard destroyed → unpinPage called

        // After destruction the frame is now unpinned.
        EXPECT_EQ(frame->pinCount(), 0);
    }

    TEST_F(PageGuardTest, BasicGuardMoveConstructorTransfersOwnership)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        PageId page_id = kInvalidPageId;
        BufferFrame* frame = nullptr;
        ASSERT_EQ(bpm.newPage(page_id, frame), Status::Ok);

        BasicPageGuard src(&bpm, frame);
        EXPECT_TRUE(src.isValid());

        BasicPageGuard dst(std::move(src));
        EXPECT_FALSE(src.isValid()); // NOLINT(bugprone-use-after-move)
        EXPECT_TRUE(dst.isValid());
        EXPECT_EQ(dst.pageId(), page_id);

        // Only one unpin should happen when dst is destroyed.
        EXPECT_EQ(frame->pinCount(), 1);
    }

    TEST_F(PageGuardTest, BasicGuardMoveAssignmentReleasesExistingOwnership)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        PageId pid1 = kInvalidPageId;
        PageId pid2 = kInvalidPageId;
        BufferFrame* frame1 = nullptr;
        BufferFrame* frame2 = nullptr;
        ASSERT_EQ(bpm.newPage(pid1, frame1), Status::Ok);
        ASSERT_EQ(bpm.newPage(pid2, frame2), Status::Ok);

        BasicPageGuard guard1(&bpm, frame1);
        BasicPageGuard guard2(&bpm, frame2);

        // Move-assign guard2 into guard1; guard1 should unpin frame1 first.
        guard1 = std::move(guard2);

        EXPECT_EQ(frame1->pinCount(), 0); // guard1's old page released
        EXPECT_EQ(frame2->pinCount(), 1); // still pinned through guard1
        EXPECT_FALSE(guard2.isValid());   // NOLINT(bugprone-use-after-move)
        EXPECT_EQ(guard1.pageId(), pid2);
    }

    TEST_F(PageGuardTest, BasicGuardDropReleasesEarly)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        PageId page_id = kInvalidPageId;
        BufferFrame* frame = nullptr;
        ASSERT_EQ(bpm.newPage(page_id, frame), Status::Ok);

        BasicPageGuard guard(&bpm, frame);
        EXPECT_TRUE(guard.isValid());
        EXPECT_EQ(frame->pinCount(), 1);

        guard.drop();

        EXPECT_FALSE(guard.isValid());
        EXPECT_EQ(frame->pinCount(), 0);

        // Second drop is a no-op (no double-unpin).
        guard.drop();
        EXPECT_EQ(frame->pinCount(), 0);
    }

    TEST_F(PageGuardTest, BasicGuardMarkDirtyPropagatesToUnpin)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        PageId page_id = kInvalidPageId;
        BufferFrame* frame = nullptr;
        ASSERT_EQ(bpm.newPage(page_id, frame), Status::Ok);

        {
            BasicPageGuard guard(&bpm, frame);
            EXPECT_FALSE(frame->isDirty());

            guard.markDirty();
            // Frame isn't dirty yet — dirty is in the guard, applied on unpin.
        } // destructor unpins with dirty=true

        // After unpin-as-dirty the frame should be marked dirty.
        EXPECT_TRUE(frame->isDirty());
    }

    TEST_F(PageGuardTest, MovedFromGuardIsInvalid)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        PageId page_id = kInvalidPageId;
        BufferFrame* frame = nullptr;
        ASSERT_EQ(bpm.newPage(page_id, frame), Status::Ok);

        BasicPageGuard guard(&bpm, frame);
        BasicPageGuard moved(std::move(guard));

        // moved-from guard must be safe to destroy without double-unpin.
        EXPECT_FALSE(guard.isValid()); // NOLINT(bugprone-use-after-move)
        guard.drop();                  // explicit drop on moved-from: no-op

        EXPECT_EQ(frame->pinCount(), 1); // still held by 'moved'
    }

    TEST_F(PageGuardTest, NestedScopesUnpinInOrder)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        PageId pid_outer = kInvalidPageId;
        PageId pid_inner = kInvalidPageId;
        BufferFrame* f_outer = nullptr;
        BufferFrame* f_inner = nullptr;

        ASSERT_EQ(bpm.newPage(pid_outer, f_outer), Status::Ok);

        {
            BasicPageGuard outer(&bpm, f_outer);
            EXPECT_EQ(f_outer->pinCount(), 1);

            ASSERT_EQ(bpm.newPage(pid_inner, f_inner), Status::Ok);

            {
                BasicPageGuard inner(&bpm, f_inner);
                EXPECT_EQ(f_inner->pinCount(), 1);
                EXPECT_EQ(f_outer->pinCount(), 1);
            } // inner released first

            EXPECT_EQ(f_inner->pinCount(), 0);
            EXPECT_EQ(f_outer->pinCount(), 1); // outer still live
        }

        EXPECT_EQ(f_outer->pinCount(), 0);
        EXPECT_EQ(f_inner->pinCount(), 0);
    }

    // =========================================================================
    // ReadPageGuard tests
    // =========================================================================

    TEST_F(PageGuardTest, ReadGuardDefaultIsInvalid)
    {
        ReadPageGuard guard;
        EXPECT_FALSE(guard.isValid());
        EXPECT_EQ(guard.pageId(), kInvalidPageId);
    }

    TEST_F(PageGuardTest, FetchPageReadReturnsValidGuard)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        // Metadata page (id=0) always exists after createDatabase.
        ReadPageGuard guard;
        ASSERT_EQ(bpm.fetchPageRead(0, guard), Status::Ok);

        EXPECT_TRUE(guard.isValid());
        EXPECT_EQ(guard.pageId(), PageId{0});
    }

    TEST_F(PageGuardTest, ReadGuardAutoUnpinsOnDestruction)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        // Allocate a page via raw API so we can check pin count afterward.
        PageId page_id = kInvalidPageId;
        BufferFrame* frame = nullptr;
        ASSERT_EQ(bpm.newPage(page_id, frame), Status::Ok);
        // Unpin the raw pin so the guard is the only holder.
        ASSERT_EQ(bpm.unpinPage(page_id, false), Status::Ok);
        EXPECT_EQ(frame->pinCount(), 0);

        {
            ReadPageGuard rg;
            ASSERT_EQ(bpm.fetchPageRead(page_id, rg), Status::Ok);
            EXPECT_EQ(frame->pinCount(), 1);
        } // destroyed → unpin

        EXPECT_EQ(frame->pinCount(), 0);
    }

    TEST_F(PageGuardTest, ReadGuardMoveSemantics)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        ReadPageGuard rg;
        ASSERT_EQ(bpm.fetchPageRead(0, rg), Status::Ok);
        EXPECT_TRUE(rg.isValid());

        ReadPageGuard rg2(std::move(rg));
        EXPECT_FALSE(rg.isValid()); // NOLINT(bugprone-use-after-move)
        EXPECT_TRUE(rg2.isValid());
        EXPECT_EQ(rg2.pageId(), PageId{0});
    }

    TEST_F(PageGuardTest, ReadGuardDropReleasesEarly)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        PageId page_id = kInvalidPageId;
        BufferFrame* frame = nullptr;
        ASSERT_EQ(bpm.newPage(page_id, frame), Status::Ok);
        ASSERT_EQ(bpm.unpinPage(page_id, false), Status::Ok);

        ReadPageGuard rg;
        ASSERT_EQ(bpm.fetchPageRead(page_id, rg), Status::Ok);
        EXPECT_EQ(frame->pinCount(), 1);

        rg.drop();
        EXPECT_FALSE(rg.isValid());
        EXPECT_EQ(frame->pinCount(), 0);
    }

    TEST_F(PageGuardTest, ReadGuardOnlyExposesConstPage)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        ReadPageGuard rg;
        ASSERT_EQ(bpm.fetchPageRead(0, rg), Status::Ok);

        // page() returns const Page&; compilation verifies const-only access.
        const Page& p = rg.page();
        EXPECT_EQ(p.data().size(), kPageSize);
    }

    // =========================================================================
    // WritePageGuard tests
    // =========================================================================

    TEST_F(PageGuardTest, WriteGuardDefaultIsInvalid)
    {
        WritePageGuard guard;
        EXPECT_FALSE(guard.isValid());
        EXPECT_EQ(guard.pageId(), kInvalidPageId);
    }

    TEST_F(PageGuardTest, NewPageGuardReturnsValidWriteGuard)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        PageId page_id = kInvalidPageId;
        WritePageGuard wg;
        ASSERT_EQ(bpm.newPageGuard(page_id, wg), Status::Ok);

        EXPECT_TRUE(wg.isValid());
        EXPECT_EQ(wg.pageId(), page_id);
        EXPECT_NE(page_id, kInvalidPageId);
    }

    TEST_F(PageGuardTest, WriteGuardAutoUnpinsOnDestruction)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        PageId page_id = kInvalidPageId;
        BufferFrame* frame = nullptr;
        ASSERT_EQ(bpm.newPage(page_id, frame), Status::Ok);
        ASSERT_EQ(bpm.unpinPage(page_id, false), Status::Ok);

        {
            WritePageGuard wg;
            ASSERT_EQ(bpm.fetchPageWrite(page_id, wg), Status::Ok);
            EXPECT_EQ(frame->pinCount(), 1);
        }

        EXPECT_EQ(frame->pinCount(), 0);
    }

    TEST_F(PageGuardTest, WriteGuardMoveSemantics)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        PageId page_id = kInvalidPageId;
        WritePageGuard wg;
        ASSERT_EQ(bpm.newPageGuard(page_id, wg), Status::Ok);
        EXPECT_TRUE(wg.isValid());

        WritePageGuard wg2(std::move(wg));
        EXPECT_FALSE(wg.isValid()); // NOLINT(bugprone-use-after-move)
        EXPECT_TRUE(wg2.isValid());
        EXPECT_EQ(wg2.pageId(), page_id);
    }

    TEST_F(PageGuardTest, WriteGuardDropReleasesEarly)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        PageId page_id = kInvalidPageId;
        BufferFrame* frame = nullptr;
        ASSERT_EQ(bpm.newPage(page_id, frame), Status::Ok);
        ASSERT_EQ(bpm.unpinPage(page_id, false), Status::Ok);

        WritePageGuard wg;
        ASSERT_EQ(bpm.fetchPageWrite(page_id, wg), Status::Ok);
        EXPECT_EQ(frame->pinCount(), 1);

        wg.drop();
        EXPECT_FALSE(wg.isValid());
        EXPECT_EQ(frame->pinCount(), 0);
    }

    TEST_F(PageGuardTest, WriteGuardMarkDirtyPropagatesToUnpin)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        PageId page_id = kInvalidPageId;
        BufferFrame* frame = nullptr;
        ASSERT_EQ(bpm.newPage(page_id, frame), Status::Ok);
        ASSERT_EQ(bpm.unpinPage(page_id, false), Status::Ok);
        EXPECT_FALSE(frame->isDirty());

        {
            WritePageGuard wg;
            ASSERT_EQ(bpm.fetchPageWrite(page_id, wg), Status::Ok);

            // Write some data and mark dirty.
            wg.pageMut().data()[PageHeader::kSize] = std::byte{'Z'};
            wg.markDirty();
        } // unpin with dirty=true

        EXPECT_TRUE(frame->isDirty());
    }

    TEST_F(PageGuardTest, WriteGuardWithoutMarkDirtyUnpinsClean)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        PageId page_id = kInvalidPageId;
        BufferFrame* frame = nullptr;
        ASSERT_EQ(bpm.newPage(page_id, frame), Status::Ok);
        ASSERT_EQ(bpm.unpinPage(page_id, false), Status::Ok);

        {
            WritePageGuard wg;
            ASSERT_EQ(bpm.fetchPageWrite(page_id, wg), Status::Ok);
            // Not calling markDirty.
        }

        EXPECT_FALSE(frame->isDirty());
    }

    TEST_F(PageGuardTest, WriteGuardExposesPageMut)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        PageId page_id = kInvalidPageId;
        WritePageGuard wg;
        ASSERT_EQ(bpm.newPageGuard(page_id, wg), Status::Ok);

        // pageMut() must return non-const Page&.
        Page& p = wg.pageMut();
        p.data()[PageHeader::kSize] = std::byte{0xAB};
        EXPECT_EQ(wg.page().data()[PageHeader::kSize], std::byte{0xAB});
    }

    // =========================================================================
    // Pin-count correctness
    // =========================================================================

    TEST_F(PageGuardTest, PinCountCorrectAfterFetchPageRead)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        PageId pid = kInvalidPageId;
        BufferFrame* frame = nullptr;
        ASSERT_EQ(bpm.newPage(pid, frame), Status::Ok);
        ASSERT_EQ(bpm.unpinPage(pid, false), Status::Ok);
        EXPECT_EQ(frame->pinCount(), 0);

        ReadPageGuard rg;
        ASSERT_EQ(bpm.fetchPageRead(pid, rg), Status::Ok);
        EXPECT_EQ(frame->pinCount(), 1);

        rg.drop();
        EXPECT_EQ(frame->pinCount(), 0);
    }

    TEST_F(PageGuardTest, PinCountCorrectAfterFetchPageWrite)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        PageId pid = kInvalidPageId;
        BufferFrame* frame = nullptr;
        ASSERT_EQ(bpm.newPage(pid, frame), Status::Ok);
        ASSERT_EQ(bpm.unpinPage(pid, false), Status::Ok);
        EXPECT_EQ(frame->pinCount(), 0);

        WritePageGuard wg;
        ASSERT_EQ(bpm.fetchPageWrite(pid, wg), Status::Ok);
        EXPECT_EQ(frame->pinCount(), 1);

        wg.drop();
        EXPECT_EQ(frame->pinCount(), 0);
    }

    TEST_F(PageGuardTest, DirtyPersistsAfterGuardDestructionAndReopen)
    {
        {
            DiskManager dm(db_path_);
            ASSERT_EQ(dm.createDatabase(), Status::Ok);
            ASSERT_EQ(dm.openDatabase(), Status::Ok);
            BufferPoolManager bpm(4, dm);

            PageId page_id = kInvalidPageId;
            WritePageGuard wg;
            ASSERT_EQ(bpm.newPageGuard(page_id, wg), Status::Ok);

            wg.pageMut().data()[PageHeader::kSize] = std::byte{0x42};
            wg.markDirty();
            wg.drop(); // dirty unpin

            ASSERT_EQ(bpm.flushAllPages(), Status::Ok);
        }

        // Reopen and verify data survived flush.
        {
            DiskManager dm(db_path_);
            ASSERT_EQ(dm.openDatabase(), Status::Ok);
            BufferPoolManager bpm(4, dm);

            ReadPageGuard rg;
            // The first user page is always page id=1 after createDatabase.
            ASSERT_EQ(bpm.fetchPageRead(1, rg), Status::Ok);
            EXPECT_EQ(rg.page().data()[PageHeader::kSize], std::byte{0x42});
        }
    }

    TEST_F(PageGuardTest, FetchPageReadFailsForNonexistentPage)
    {
        DiskManager dm(db_path_);
        ASSERT_EQ(dm.createDatabase(), Status::Ok);
        ASSERT_EQ(dm.openDatabase(), Status::Ok);
        BufferPoolManager bpm(4, dm);

        ReadPageGuard rg;
        // Page 999 has never been allocated — should fail with IOError or NotFound.
        Status s = bpm.fetchPageRead(999, rg);
        EXPECT_NE(s, Status::Ok);
        EXPECT_FALSE(rg.isValid());
    }

} // namespace hamdb
