#include "common/enums.hpp"
#include "storage/page.hpp"
#include "storage/page_header.hpp"
#include "storage/slotted_page.hpp"
#include "storage/tuple_slot.hpp"
#include "storage/tuple.hpp"
#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <span>
#include <string_view>
#include <vector>

namespace hamdb
{

    // ─────────────────────────────────────────────────────────────────────────
    // Helpers
    // ─────────────────────────────────────────────────────────────────────────

    /// Convert a string_view to a Tuple (safe, no UB — char is aliasable).
    static Tuple toTuple(std::string_view sv) noexcept
    {
        std::span<const std::byte> span{reinterpret_cast<const std::byte*>(sv.data()), sv.size()}; // NOLINT
        return Tuple(span);
    }

    /// Convert Tuple bytes back to string_view for easy comparison in tests.
    static std::string_view fromTuple(const Tuple& t) noexcept
    {
        auto s = t.data();
        return {reinterpret_cast<const char*>(s.data()), s.size()}; // NOLINT
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test fixture — owns Page so SlottedPage reference stays valid
    // ─────────────────────────────────────────────────────────────────────────

    class SlottedPageTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            page_ = std::make_unique<Page>(PageHeader(1, PageType::Table));
            sp_   = std::make_unique<SlottedPage>(*page_);
            ASSERT_EQ(sp_->initialize(), Status::Ok);
        }

        std::unique_ptr<Page>        page_;
        std::unique_ptr<SlottedPage> sp_;
    };

    // ─────────────────────────────────────────────────────────────────────────
    // TupleSlot unit tests
    // ─────────────────────────────────────────────────────────────────────────

    TEST(TupleSlotTest, SizeIsEightBytes)
    {
        EXPECT_EQ(TupleSlot::kSize, 8u);
        EXPECT_EQ(sizeof(TupleSlot), 8u);
    }

    TEST(TupleSlotTest, DefaultConstructedIsNotDeleted)
    {
        TupleSlot slot;
        EXPECT_FALSE(slot.isDeleted());
    }

    TEST(TupleSlotTest, MarkDeletedSetsFlag)
    {
        TupleSlot slot;
        slot.markDeleted();
        EXPECT_TRUE(slot.isDeleted());
    }

    TEST(TupleSlotTest, ClearDeletedClearsFlag)
    {
        TupleSlot slot;
        slot.markDeleted();
        slot.clearDeleted();
        EXPECT_FALSE(slot.isDeleted());
    }

    TEST(TupleSlotTest, ParameterizedConstructorSetsFields)
    {
        TupleSlot slot(100u, 32u);
        EXPECT_EQ(slot.offset, 100u);
        EXPECT_EQ(slot.length, 32u);
        EXPECT_FALSE(slot.isDeleted());
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Empty page after initialize()
    // ─────────────────────────────────────────────────────────────────────────

    TEST_F(SlottedPageTest, InitialSlotCountIsZero)
    {
        EXPECT_EQ(sp_->slotCount(), 0u);
    }

    TEST_F(SlottedPageTest, InitialTupleCountIsZero)
    {
        EXPECT_EQ(sp_->tupleCount(), 0u);
    }

    TEST_F(SlottedPageTest, InitialFreeSpaceIsBodyMinusHeader)
    {
        // Free space = body (4080) - SlottedPageHeader (16)
        const std::size_t expected = Page::kBodySize - SlottedPageHeader::kSize;
        EXPECT_EQ(sp_->freeSpace(), expected);
    }

    TEST_F(SlottedPageTest, IsNotFullForSmallTuple)
    {
        EXPECT_FALSE(sp_->isFull(16));
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Single insert
    // ─────────────────────────────────────────────────────────────────────────

    TEST_F(SlottedPageTest, InsertSingleTupleReturnsOkAndSlotZero)
    {
        SlotId id = kInvalidSlotId;
        EXPECT_EQ(sp_->insertTuple(toTuple("hello"), id), Status::Ok);
        EXPECT_EQ(id, 0u);
    }

    TEST_F(SlottedPageTest, InsertIncreasesSlotCount)
    {
        SlotId id = kInvalidSlotId;
        ASSERT_EQ(sp_->insertTuple(toTuple("data"), id), Status::Ok);
        EXPECT_EQ(sp_->slotCount(), 1u);
    }

    TEST_F(SlottedPageTest, InsertIncreasesTupleCount)
    {
        SlotId id = kInvalidSlotId;
        ASSERT_EQ(sp_->insertTuple(toTuple("data"), id), Status::Ok);
        EXPECT_EQ(sp_->tupleCount(), 1u);
    }

    TEST_F(SlottedPageTest, InsertReducesFreeSpace)
    {
        const std::size_t before = sp_->freeSpace();
        SlotId id = kInvalidSlotId;
        ASSERT_EQ(sp_->insertTuple(toTuple("hello"), id), Status::Ok);
        const std::size_t after = sp_->freeSpace();
        // consumed: 5 bytes payload + 8 bytes slot
        EXPECT_EQ(before - after, 5u + TupleSlot::kSize);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Read tuple
    // ─────────────────────────────────────────────────────────────────────────

    TEST_F(SlottedPageTest, ReadTupleMatchesInserted)
    {
        SlotId id = kInvalidSlotId;
        ASSERT_EQ(sp_->insertTuple(toTuple("hamdb"), id), Status::Ok);

        Tuple out;
        ASSERT_EQ(sp_->readTuple(id, out), Status::Ok);
        EXPECT_EQ(fromTuple(out), "hamdb");
    }

    TEST_F(SlottedPageTest, ReadInvalidSlotReturnsNotFound)
    {
        Tuple out;
        EXPECT_EQ(sp_->readTuple(99u, out), Status::NotFound);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Multiple inserts
    // ─────────────────────────────────────────────────────────────────────────

    TEST_F(SlottedPageTest, MultipleInsertAssignsSequentialSlotIds)
    {
        const std::vector<std::string_view> tuples = {"one", "two", "three", "four"};
        for (std::size_t i = 0; i < tuples.size(); ++i)
        {
            SlotId id = kInvalidSlotId;
            ASSERT_EQ(sp_->insertTuple(toTuple(tuples[i]), id), Status::Ok);
            EXPECT_EQ(id, static_cast<SlotId>(i));
        }
        EXPECT_EQ(sp_->slotCount(),  4u);
        EXPECT_EQ(sp_->tupleCount(), 4u);
    }

    TEST_F(SlottedPageTest, MultipleInsertReadBackAllTuples)
    {
        const std::vector<std::string_view> tuples = {"alpha", "beta", "gamma"};
        std::vector<SlotId> ids(tuples.size());
        for (std::size_t i = 0; i < tuples.size(); ++i)
        {
            ASSERT_EQ(sp_->insertTuple(toTuple(tuples[i]), ids[i]), Status::Ok);
        }
        for (std::size_t i = 0; i < tuples.size(); ++i)
        {
            Tuple out;
            ASSERT_EQ(sp_->readTuple(ids[i], out), Status::Ok);
            EXPECT_EQ(fromTuple(out), tuples[i]);
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Delete tuple
    // ─────────────────────────────────────────────────────────────────────────

    TEST_F(SlottedPageTest, DeleteTupleReturnsOk)
    {
        SlotId id = kInvalidSlotId;
        ASSERT_EQ(sp_->insertTuple(toTuple("to_delete"), id), Status::Ok);
        EXPECT_EQ(sp_->deleteTuple(id), Status::Ok);
    }

    TEST_F(SlottedPageTest, DeleteDecreasesTupleCountNotSlotCount)
    {
        SlotId id = kInvalidSlotId;
        ASSERT_EQ(sp_->insertTuple(toTuple("x"), id), Status::Ok);
        ASSERT_EQ(sp_->deleteTuple(id), Status::Ok);
        EXPECT_EQ(sp_->slotCount(),  1u); // slot dir entry remains
        EXPECT_EQ(sp_->tupleCount(), 0u); // live tuple count drops
    }

    TEST_F(SlottedPageTest, ReadDeletedSlotReturnsInvalidArg)
    {
        SlotId id = kInvalidSlotId;
        ASSERT_EQ(sp_->insertTuple(toTuple("gone"), id), Status::Ok);
        ASSERT_EQ(sp_->deleteTuple(id), Status::Ok);

        Tuple out;
        EXPECT_EQ(sp_->readTuple(id, out), Status::InvalidArg);
    }

    TEST_F(SlottedPageTest, DeleteAlreadyDeletedReturnsInvalidArg)
    {
        SlotId id = kInvalidSlotId;
        ASSERT_EQ(sp_->insertTuple(toTuple("once"), id), Status::Ok);
        ASSERT_EQ(sp_->deleteTuple(id), Status::Ok);
        EXPECT_EQ(sp_->deleteTuple(id), Status::InvalidArg);
    }

    TEST_F(SlottedPageTest, DeleteNonExistentSlotReturnsNotFound)
    {
        EXPECT_EQ(sp_->deleteTuple(42u), Status::NotFound);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Reuse deleted slot
    // ─────────────────────────────────────────────────────────────────────────

    TEST_F(SlottedPageTest, InsertAfterDeleteReusesSlot)
    {
        SlotId id0 = kInvalidSlotId;
        ASSERT_EQ(sp_->insertTuple(toTuple("first"), id0), Status::Ok);
        ASSERT_EQ(sp_->deleteTuple(id0), Status::Ok);

        SlotId id1 = kInvalidSlotId;
        ASSERT_EQ(sp_->insertTuple(toTuple("second"), id1), Status::Ok);

        // Reused slot: slot count stays at 1, not 2
        EXPECT_EQ(id1, id0);
        EXPECT_EQ(sp_->slotCount(), 1u);
        EXPECT_EQ(sp_->tupleCount(), 1u);
    }

    TEST_F(SlottedPageTest, ReuseSlotPreservesReadability)
    {
        SlotId id0 = kInvalidSlotId;
        ASSERT_EQ(sp_->insertTuple(toTuple("old"), id0), Status::Ok);
        ASSERT_EQ(sp_->deleteTuple(id0), Status::Ok);

        SlotId id1 = kInvalidSlotId;
        ASSERT_EQ(sp_->insertTuple(toTuple("new"), id1), Status::Ok);

        Tuple out;
        ASSERT_EQ(sp_->readTuple(id1, out), Status::Ok);
        EXPECT_EQ(fromTuple(out), "new");
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Page full detection
    // ─────────────────────────────────────────────────────────────────────────

    TEST_F(SlottedPageTest, IsFullReturnsTrueWhenNoSpace)
    {
        // Fill the page with as many small tuples as will fit
        std::array<std::byte, 1> tiny{std::byte{0xFF}};
        Tuple tuple(tiny);
        SlotId id = kInvalidSlotId;
        while (sp_->insertTuple(tuple, id) == Status::Ok)
        {
        }
        EXPECT_TRUE(sp_->isFull(1));
    }

    TEST_F(SlottedPageTest, InsertOnFullPageReturnsIoError)
    {
        std::array<std::byte, 1> tiny{std::byte{0}};
        Tuple tuple(tiny);
        SlotId id = kInvalidSlotId;
        while (sp_->insertTuple(tuple, id) == Status::Ok)
        {
        }
        EXPECT_EQ(sp_->insertTuple(tuple, id), Status::IoError);
    }

    TEST_F(SlottedPageTest, EmptyTupleReturnsInvalidArg)
    {
        SlotId id = kInvalidSlotId;
        Tuple empty;
        EXPECT_EQ(sp_->insertTuple(empty, id), Status::InvalidArg);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Compaction
    // ─────────────────────────────────────────────────────────────────────────

    TEST_F(SlottedPageTest, CompactOnEmptyPageReclainsZero)
    {
        std::size_t reclaimed = 99u;
        ASSERT_EQ(sp_->compact(reclaimed), Status::Ok);
        EXPECT_EQ(reclaimed, 0u);
    }

    TEST_F(SlottedPageTest, CompactAfterDeleteReclaimsBytes)
    {
        SlotId id0 = kInvalidSlotId;
        SlotId id1 = kInvalidSlotId;
        ASSERT_EQ(sp_->insertTuple(toTuple("AAAAAAAAAA"), id0), Status::Ok); // 10 bytes
        ASSERT_EQ(sp_->insertTuple(toTuple("BBBBBBBBBB"), id1), Status::Ok); // 10 bytes
        ASSERT_EQ(sp_->deleteTuple(id0), Status::Ok);

        const std::size_t free_before = sp_->freeSpace();
        std::size_t reclaimed = 0;
        ASSERT_EQ(sp_->compact(reclaimed), Status::Ok);
        const std::size_t free_after = sp_->freeSpace();

        EXPECT_EQ(reclaimed, 10u); // deleted tuple recovered
        EXPECT_GT(free_after, free_before);
    }

    TEST_F(SlottedPageTest, CompactPreservesLiveTupleData)
    {
        SlotId id0 = kInvalidSlotId;
        SlotId id1 = kInvalidSlotId;
        SlotId id2 = kInvalidSlotId;
        ASSERT_EQ(sp_->insertTuple(toTuple("keep_a"),  id0), Status::Ok);
        ASSERT_EQ(sp_->insertTuple(toTuple("delete_b"), id1), Status::Ok);
        ASSERT_EQ(sp_->insertTuple(toTuple("keep_c"),  id2), Status::Ok);
        ASSERT_EQ(sp_->deleteTuple(id1), Status::Ok);

        std::size_t reclaimed = 0;
        ASSERT_EQ(sp_->compact(reclaimed), Status::Ok);

        Tuple out;
        ASSERT_EQ(sp_->readTuple(id0, out), Status::Ok);
        EXPECT_EQ(fromTuple(out), "keep_a");

        ASSERT_EQ(sp_->readTuple(id2, out), Status::Ok);
        EXPECT_EQ(fromTuple(out), "keep_c");
    }

    TEST_F(SlottedPageTest, CompactPreservesSlotIndices)
    {
        SlotId id0 = kInvalidSlotId;
        SlotId id1 = kInvalidSlotId;
        ASSERT_EQ(sp_->insertTuple(toTuple("X"), id0), Status::Ok); // slot 0
        ASSERT_EQ(sp_->insertTuple(toTuple("Y"), id1), Status::Ok); // slot 1
        ASSERT_EQ(sp_->deleteTuple(id0), Status::Ok);

        std::size_t reclaimed = 0;
        ASSERT_EQ(sp_->compact(reclaimed), Status::Ok);

        // slot 1 still readable at the same slot id
        Tuple out;
        ASSERT_EQ(sp_->readTuple(id1, out), Status::Ok);
        EXPECT_EQ(fromTuple(out), "Y");

        // slot 0 still reports deleted
        EXPECT_EQ(sp_->readTuple(id0, out), Status::InvalidArg);
    }

    TEST_F(SlottedPageTest, CompactAllowsNewInsertsAfterFragmentation)
    {
        // Fill to near capacity with medium tuples
        std::array<std::byte, 64> chunk{};
        Tuple tuple(chunk);
        std::vector<SlotId> ids;
        SlotId id = kInvalidSlotId;
        while (sp_->insertTuple(tuple, id) == Status::Ok)
        {
            ids.push_back(id);
        }

        // Delete every other tuple to fragment the page
        for (std::size_t i = 0; i < ids.size(); i += 2)
        {
            ASSERT_EQ(sp_->deleteTuple(ids[i]), Status::Ok);
        }

        // Before compact, inserting a large chunk may fail
        std::size_t reclaimed = 0;
        ASSERT_EQ(sp_->compact(reclaimed), Status::Ok);
        EXPECT_GT(reclaimed, 0u);

        // After compact we should have enough contiguous space for a new chunk
        SlotId new_id = kInvalidSlotId;
        EXPECT_EQ(sp_->insertTuple(tuple, new_id), Status::Ok);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Slot count and tuple count invariants
    // ─────────────────────────────────────────────────────────────────────────

    TEST_F(SlottedPageTest, SlotCountNeverDecreasesOnDelete)
    {
        SlotId id = kInvalidSlotId;
        ASSERT_EQ(sp_->insertTuple(toTuple("a"), id), Status::Ok);
        ASSERT_EQ(sp_->insertTuple(toTuple("b"), id), Status::Ok);
        ASSERT_EQ(sp_->deleteTuple(0u), Status::Ok);
        EXPECT_EQ(sp_->slotCount(), 2u); // dir entries stay
        EXPECT_EQ(sp_->tupleCount(), 1u);
    }

    TEST_F(SlottedPageTest, TupleCountTracksLiveTuplesOnly)
    {
        std::vector<SlotId> ids;
        for (int i = 0; i < 5; ++i)
        {
            SlotId id = kInvalidSlotId;
            ASSERT_EQ(sp_->insertTuple(toTuple("x"), id), Status::Ok);
            ids.push_back(id);
        }
        EXPECT_EQ(sp_->tupleCount(), 5u);

        ASSERT_EQ(sp_->deleteTuple(ids[1]), Status::Ok);
        ASSERT_EQ(sp_->deleteTuple(ids[3]), Status::Ok);
        EXPECT_EQ(sp_->tupleCount(), 3u);

        std::size_t reclaimed = 0;
        ASSERT_EQ(sp_->compact(reclaimed), Status::Ok);
        EXPECT_EQ(sp_->tupleCount(), 3u); // compact doesn't change count
    }

} // namespace hamdb
