/// @file btree_leaf_page_test.cpp
/// @brief Unit tests for BTreeLeafPage — the B+ Tree leaf node.
///
/// Test coverage:
///   - Initialisation (init, defaults)
///   - Size / capacity predicates
///   - Sibling page link getters/setters
///   - Sorted insertion (single, multiple, lower/upper boundary, all-ascending,
///     all-descending, random order)
///   - Duplicate key rejection
///   - Full-page rejection
///   - Slot accessors (keyAt / valueAt)
///   - Binary-search lookup (found / not found / edge cases)
///   - Deletion (middle, first, last, not found, re-insert after delete)
///   - Serialisation round-trip (empty page, partial, full)
///   - Serialisation error handling (buffer too small)
///   - Deserialisation error handling (corrupt / short buffer)
///   - Leaf link round-trip through serialisation
///   - Layout constants sanity

#include "index/btree_leaf_page.hpp"

#include "common/constants.hpp"
#include "common/enums.hpp"
#include "storage/rid.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <gtest/gtest.h>

namespace hamdb
{

// ── Helpers ───────────────────────────────────────────────────────────────────

namespace
{
    constexpr PageId kPage1    = 1;
    constexpr PageId kPage2    = 2;
    constexpr PageId kPage3    = 3;
    constexpr PageId kNoParent = kInvalidPageId;

    /// Create a simple RID from slot index @p s (page=kPage1).
    RID makeRid(uint16_t s)
    {
        return RID{kPage1, s};
    }

    /// Build a fully-initialised leaf page.
    BTreeLeafPage makePage(PageId page_id = kPage1, PageId parent = kNoParent)
    {
        BTreeLeafPage lp;
        lp.init(page_id, parent);
        return lp;
    }

    /// Insert @p key into @p lp, asserting success.
    void mustInsert(BTreeLeafPage& lp, int64_t key, RID rid)
    {
        ASSERT_EQ(lp.insert(key, rid), Status::Ok) << "insert(" << key << ") failed";
    }

    /// Remove @p key from @p lp, asserting success.
    void mustRemove(BTreeLeafPage& lp, int64_t key)
    {
        ASSERT_EQ(lp.remove(key), Status::Ok) << "remove(" << key << ") failed";
    }
} // anonymous namespace

// ── Layout constants ──────────────────────────────────────────────────────────

TEST(BTreeLeafPageTest, LeafHeaderSizeIs24)
{
    EXPECT_EQ(BTreeLeafPage::kLeafHeaderSize, 24u);
}

TEST(BTreeLeafPageTest, EntrySizeIs14)
{
    EXPECT_EQ(BTreeLeafPage::kEntrySize, 14u);
}

TEST(BTreeLeafPageTest, MaxEntriesFitsInPageBody)
{
    // The full slot array must not overflow kPageBodySize.
    const std::size_t used =
        BTreeLeafPage::kLeafHeaderSize +
        static_cast<std::size_t>(BTreeLeafPage::kMaxEntries) * BTreeLeafPage::kEntrySize;
    EXPECT_LE(used, kPageBodySize);
}

TEST(BTreeLeafPageTest, MaxEntriesIsNonZero)
{
    EXPECT_GT(BTreeLeafPage::kMaxEntries, 0u);
}

// ── Default construction ──────────────────────────────────────────────────────

TEST(BTreeLeafPageTest, DefaultConstructedIsEmpty)
{
    BTreeLeafPage lp;
    EXPECT_EQ(lp.size(), 0u);
}

TEST(BTreeLeafPageTest, DefaultConstructedPrevIsInvalid)
{
    BTreeLeafPage lp;
    EXPECT_EQ(lp.prevPageId(), kInvalidPageId);
}

TEST(BTreeLeafPageTest, DefaultConstructedNextIsInvalid)
{
    BTreeLeafPage lp;
    EXPECT_EQ(lp.nextPageId(), kInvalidPageId);
}

// ── Initialisation ────────────────────────────────────────────────────────────

TEST(BTreeLeafPageTest, InitSetsPageId)
{
    auto lp = makePage(kPage2);
    EXPECT_EQ(lp.pageId(), kPage2);
}

TEST(BTreeLeafPageTest, InitSetsParentPageId)
{
    auto lp = makePage(kPage1, kPage3);
    EXPECT_EQ(lp.parentPageId(), kPage3);
}

TEST(BTreeLeafPageTest, InitSetsParentToInvalidForRoot)
{
    auto lp = makePage(kPage1, kNoParent);
    EXPECT_EQ(lp.parentPageId(), kInvalidPageId);
}

TEST(BTreeLeafPageTest, InitCurrentSizeIsZero)
{
    auto lp = makePage();
    EXPECT_EQ(lp.size(), 0u);
}

TEST(BTreeLeafPageTest, InitMaxSizeEqualsMaxEntries)
{
    auto lp = makePage();
    EXPECT_EQ(lp.maxSize(), static_cast<uint16_t>(BTreeLeafPage::kMaxEntries));
}

TEST(BTreeLeafPageTest, InitIsEmpty)
{
    auto lp = makePage();
    EXPECT_TRUE(lp.isEmpty());
}

TEST(BTreeLeafPageTest, InitIsNotFull)
{
    auto lp = makePage();
    EXPECT_FALSE(lp.isFull());
}

TEST(BTreeLeafPageTest, InitPrevIsInvalid)
{
    auto lp = makePage();
    EXPECT_EQ(lp.prevPageId(), kInvalidPageId);
}

TEST(BTreeLeafPageTest, InitNextIsInvalid)
{
    auto lp = makePage();
    EXPECT_EQ(lp.nextPageId(), kInvalidPageId);
}

// ── Sibling page link accessors ────────────────────────────────────────────────

TEST(BTreeLeafPageTest, SetPrevPageId)
{
    auto lp = makePage();
    lp.setPrevPageId(kPage2);
    EXPECT_EQ(lp.prevPageId(), kPage2);
}

TEST(BTreeLeafPageTest, SetNextPageId)
{
    auto lp = makePage();
    lp.setNextPageId(kPage3);
    EXPECT_EQ(lp.nextPageId(), kPage3);
}

TEST(BTreeLeafPageTest, SetPrevToInvalidSentinel)
{
    auto lp = makePage();
    lp.setPrevPageId(kPage2);
    lp.setPrevPageId(kInvalidPageId);
    EXPECT_EQ(lp.prevPageId(), kInvalidPageId);
}

TEST(BTreeLeafPageTest, SetNextToInvalidSentinel)
{
    auto lp = makePage();
    lp.setNextPageId(kPage3);
    lp.setNextPageId(kInvalidPageId);
    EXPECT_EQ(lp.nextPageId(), kInvalidPageId);
}

TEST(BTreeLeafPageTest, PrevAndNextAreIndependent)
{
    auto lp = makePage();
    lp.setPrevPageId(10);
    lp.setNextPageId(20);
    EXPECT_EQ(lp.prevPageId(), 10u);
    EXPECT_EQ(lp.nextPageId(), 20u);
}

// ── Sorted insertion ──────────────────────────────────────────────────────────

TEST(BTreeLeafPageTest, InsertSingleEntry)
{
    auto lp = makePage();
    EXPECT_EQ(lp.insert(42, makeRid(0)), Status::Ok);
    EXPECT_EQ(lp.size(), 1u);
}

TEST(BTreeLeafPageTest, InsertSingleEntryNotEmptyAfterward)
{
    auto lp = makePage();
    ASSERT_EQ(lp.insert(42, makeRid(0)), Status::Ok);
    EXPECT_FALSE(lp.isEmpty());
}

TEST(BTreeLeafPageTest, InsertTwoEntriesAscendingOrder)
{
    auto lp = makePage();
    EXPECT_EQ(lp.insert(10, makeRid(0)), Status::Ok);
    EXPECT_EQ(lp.insert(20, makeRid(1)), Status::Ok);
    EXPECT_EQ(lp.size(), 2u);
    EXPECT_EQ(lp.keyAt(0), 10);
    EXPECT_EQ(lp.keyAt(1), 20);
}

TEST(BTreeLeafPageTest, InsertTwoEntriesDescendingOrder)
{
    auto lp = makePage();
    ASSERT_EQ(lp.insert(20, makeRid(0)), Status::Ok);
    ASSERT_EQ(lp.insert(10, makeRid(1)), Status::Ok);
    EXPECT_EQ(lp.size(), 2u);
    EXPECT_EQ(lp.keyAt(0), 10);
    EXPECT_EQ(lp.keyAt(1), 20);
}

TEST(BTreeLeafPageTest, InsertMaintainsSortedOrder)
{
    auto lp = makePage();
    // Insert in non-sorted order.
    mustInsert(lp, 30, makeRid(2));
    mustInsert(lp, 10, makeRid(0));
    mustInsert(lp, 50, makeRid(4));
    mustInsert(lp, 20, makeRid(1));
    mustInsert(lp, 40, makeRid(3));

    ASSERT_EQ(lp.size(), 5u);
    EXPECT_EQ(lp.keyAt(0), 10);
    EXPECT_EQ(lp.keyAt(1), 20);
    EXPECT_EQ(lp.keyAt(2), 30);
    EXPECT_EQ(lp.keyAt(3), 40);
    EXPECT_EQ(lp.keyAt(4), 50);
}

TEST(BTreeLeafPageTest, InsertNegativeKeys)
{
    auto lp = makePage();
    mustInsert(lp, -10, makeRid(0));
    mustInsert(lp, -30, makeRid(1));
    mustInsert(lp, -20, makeRid(2));

    ASSERT_EQ(lp.size(), 3u);
    EXPECT_EQ(lp.keyAt(0), -30);
    EXPECT_EQ(lp.keyAt(1), -20);
    EXPECT_EQ(lp.keyAt(2), -10);
}

TEST(BTreeLeafPageTest, InsertMixedNegativePositiveKeys)
{
    auto lp = makePage();
    mustInsert(lp,  5, makeRid(2));
    mustInsert(lp, -5, makeRid(0));
    mustInsert(lp,  0, makeRid(1));

    ASSERT_EQ(lp.size(), 3u);
    EXPECT_EQ(lp.keyAt(0), -5);
    EXPECT_EQ(lp.keyAt(1),  0);
    EXPECT_EQ(lp.keyAt(2),  5);
}

TEST(BTreeLeafPageTest, InsertSmallestKeyBecomesFirst)
{
    auto lp = makePage();
    mustInsert(lp, 100, makeRid(1));
    mustInsert(lp, 200, makeRid(2));
    mustInsert(lp,  -1, makeRid(0));

    EXPECT_EQ(lp.keyAt(0), -1);
}

TEST(BTreeLeafPageTest, InsertLargestKeyBecomesLast)
{
    auto lp = makePage();
    mustInsert(lp,  10, makeRid(0));
    mustInsert(lp,  20, makeRid(1));
    mustInsert(lp, 999, makeRid(2));

    EXPECT_EQ(lp.keyAt(2), 999);
}

// ── Duplicate key rejection ───────────────────────────────────────────────────

TEST(BTreeLeafPageTest, InsertDuplicateKeyReturnsAlreadyExists)
{
    auto lp = makePage();
    ASSERT_EQ(lp.insert(42, makeRid(0)), Status::Ok);
    EXPECT_EQ(lp.insert(42, makeRid(1)), Status::AlreadyExists);
}

TEST(BTreeLeafPageTest, InsertDuplicateDoesNotChangeSize)
{
    auto lp = makePage();
    ASSERT_EQ(lp.insert(42, makeRid(0)), Status::Ok);
    static_cast<void>(lp.insert(42, makeRid(1))); // duplicate — expected to fail
    EXPECT_EQ(lp.size(), 1u);
}

TEST(BTreeLeafPageTest, InsertDuplicateDoesNotChangeExistingRid)
{
    auto lp = makePage();
    const RID original = makeRid(5);
    ASSERT_EQ(lp.insert(42, original), Status::Ok);
    static_cast<void>(lp.insert(42, makeRid(99))); // duplicate — expected to fail
    EXPECT_EQ(lp.valueAt(0), original);
}

// ── Full-page behaviour ───────────────────────────────────────────────────────

TEST(BTreeLeafPageTest, IsFullAfterMaxEntries)
{
    auto lp = makePage();
    for (int64_t k = 0; k < static_cast<int64_t>(BTreeLeafPage::kMaxEntries); ++k)
    {
        mustInsert(lp, k, makeRid(static_cast<uint16_t>(k)));
    }
    EXPECT_TRUE(lp.isFull());
}

TEST(BTreeLeafPageTest, InsertIntoFullPageReturnsInvalidArg)
{
    auto lp = makePage();
    for (int64_t k = 0; k < static_cast<int64_t>(BTreeLeafPage::kMaxEntries); ++k)
    {
        mustInsert(lp, k, makeRid(static_cast<uint16_t>(k)));
    }
    EXPECT_EQ(lp.insert(INT64_MAX, makeRid(0)), Status::InvalidArg);
}

TEST(BTreeLeafPageTest, SizeEqualsMaxEntriesWhenFull)
{
    auto lp = makePage();
    for (int64_t k = 0; k < static_cast<int64_t>(BTreeLeafPage::kMaxEntries); ++k)
    {
        mustInsert(lp, k, makeRid(0));
    }
    EXPECT_EQ(lp.size(), lp.maxSize());
}

// ── Slot accessors ────────────────────────────────────────────────────────────

TEST(BTreeLeafPageTest, KeyAtReturnsCorrectKey)
{
    auto lp = makePage();
    ASSERT_EQ(lp.insert(7, makeRid(0)), Status::Ok);
    EXPECT_EQ(lp.keyAt(0), 7);
}

TEST(BTreeLeafPageTest, ValueAtReturnsCorrectRid)
{
    auto lp = makePage();
    const RID rid{kPage2, 3};
    ASSERT_EQ(lp.insert(7, rid), Status::Ok);
    EXPECT_EQ(lp.valueAt(0), rid);
}

TEST(BTreeLeafPageTest, KeyAndValueAtMultipleSlots)
{
    auto lp = makePage();
    mustInsert(lp, 100, RID{kPage1, 0});
    mustInsert(lp, 200, RID{kPage2, 1});
    mustInsert(lp, 300, RID{kPage3, 2});

    EXPECT_EQ(lp.keyAt(0), 100); EXPECT_EQ(lp.valueAt(0), (RID{kPage1, 0}));
    EXPECT_EQ(lp.keyAt(1), 200); EXPECT_EQ(lp.valueAt(1), (RID{kPage2, 1}));
    EXPECT_EQ(lp.keyAt(2), 300); EXPECT_EQ(lp.valueAt(2), (RID{kPage3, 2}));
}

// ── Binary-search lookup ──────────────────────────────────────────────────────

TEST(BTreeLeafPageTest, LookupOnEmptyPageReturnsNullopt)
{
    auto lp = makePage();
    EXPECT_FALSE(lp.lookup(42).has_value());
}

TEST(BTreeLeafPageTest, LookupExistingKeyReturnsRid)
{
    auto lp = makePage();
    const RID rid{kPage2, 7};
    ASSERT_EQ(lp.insert(42, rid), Status::Ok);
    auto result = lp.lookup(42);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, rid);
}

TEST(BTreeLeafPageTest, LookupMissingKeyReturnsNullopt)
{
    auto lp = makePage();
    mustInsert(lp, 10, makeRid(0));
    mustInsert(lp, 20, makeRid(1));
    EXPECT_FALSE(lp.lookup(15).has_value());
}

TEST(BTreeLeafPageTest, LookupFirstKey)
{
    auto lp = makePage();
    mustInsert(lp, 1, makeRid(0));
    mustInsert(lp, 2, makeRid(1));
    mustInsert(lp, 3, makeRid(2));
    auto result = lp.lookup(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, makeRid(0));
}

TEST(BTreeLeafPageTest, LookupLastKey)
{
    auto lp = makePage();
    mustInsert(lp, 1, makeRid(0));
    mustInsert(lp, 2, makeRid(1));
    mustInsert(lp, 3, makeRid(2));
    auto result = lp.lookup(3);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, makeRid(2));
}

TEST(BTreeLeafPageTest, LookupMiddleKey)
{
    auto lp = makePage();
    mustInsert(lp, 10, makeRid(0));
    mustInsert(lp, 20, makeRid(1));
    mustInsert(lp, 30, makeRid(2));
    auto result = lp.lookup(20);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, makeRid(1));
}

TEST(BTreeLeafPageTest, LookupKeyBelowRangeReturnsNullopt)
{
    auto lp = makePage();
    mustInsert(lp, 10, makeRid(0));
    EXPECT_FALSE(lp.lookup(5).has_value());
}

TEST(BTreeLeafPageTest, LookupKeyAboveRangeReturnsNullopt)
{
    auto lp = makePage();
    mustInsert(lp, 10, makeRid(0));
    EXPECT_FALSE(lp.lookup(100).has_value());
}

TEST(BTreeLeafPageTest, LookupNegativeKey)
{
    auto lp = makePage();
    const RID rid{kPage1, 9};
    mustInsert(lp, -55, rid);
    auto result = lp.lookup(-55);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, rid);
}

// ── Deletion ──────────────────────────────────────────────────────────────────

TEST(BTreeLeafPageTest, RemoveSingleEntry)
{
    auto lp = makePage();
    mustInsert(lp, 42, makeRid(0));
    EXPECT_EQ(lp.remove(42), Status::Ok);
    EXPECT_TRUE(lp.isEmpty());
}

TEST(BTreeLeafPageTest, RemoveDecreasesSize)
{
    auto lp = makePage();
    mustInsert(lp, 10, makeRid(0));
    mustInsert(lp, 20, makeRid(1));
    mustRemove(lp, 10);
    EXPECT_EQ(lp.size(), 1u);
}

TEST(BTreeLeafPageTest, RemoveNotFoundReturnsNotFound)
{
    auto lp = makePage();
    mustInsert(lp, 10, makeRid(0));
    EXPECT_EQ(lp.remove(99), Status::NotFound);
}

TEST(BTreeLeafPageTest, RemoveFromEmptyPageReturnsNotFound)
{
    auto lp = makePage();
    EXPECT_EQ(lp.remove(1), Status::NotFound);
}

TEST(BTreeLeafPageTest, RemoveFirstKeepsSortedOrder)
{
    auto lp = makePage();
    mustInsert(lp, 10, makeRid(0));
    mustInsert(lp, 20, makeRid(1));
    mustInsert(lp, 30, makeRid(2));
    mustRemove(lp, 10);

    ASSERT_EQ(lp.size(), 2u);
    EXPECT_EQ(lp.keyAt(0), 20);
    EXPECT_EQ(lp.keyAt(1), 30);
}

TEST(BTreeLeafPageTest, RemoveLastKeepsSortedOrder)
{
    auto lp = makePage();
    mustInsert(lp, 10, makeRid(0));
    mustInsert(lp, 20, makeRid(1));
    mustInsert(lp, 30, makeRid(2));
    mustRemove(lp, 30);

    ASSERT_EQ(lp.size(), 2u);
    EXPECT_EQ(lp.keyAt(0), 10);
    EXPECT_EQ(lp.keyAt(1), 20);
}

TEST(BTreeLeafPageTest, RemoveMiddleKeepsSortedOrder)
{
    auto lp = makePage();
    mustInsert(lp, 10, makeRid(0));
    mustInsert(lp, 20, makeRid(1));
    mustInsert(lp, 30, makeRid(2));
    mustRemove(lp, 20);

    ASSERT_EQ(lp.size(), 2u);
    EXPECT_EQ(lp.keyAt(0), 10);
    EXPECT_EQ(lp.keyAt(1), 30);
}

TEST(BTreeLeafPageTest, RemoveDoesNotChangeRemainingRids)
{
    auto lp = makePage();
    mustInsert(lp, 10, RID{kPage1, 0});
    mustInsert(lp, 20, RID{kPage2, 1});
    mustInsert(lp, 30, RID{kPage3, 2});
    mustRemove(lp, 20);

    EXPECT_EQ(lp.valueAt(0), (RID{kPage1, 0}));
    EXPECT_EQ(lp.valueAt(1), (RID{kPage3, 2}));
}

TEST(BTreeLeafPageTest, ReinsertAfterRemoveSucceeds)
{
    auto lp = makePage();
    mustInsert(lp, 42, makeRid(0));
    mustRemove(lp, 42);
    EXPECT_EQ(lp.insert(42, makeRid(1)), Status::Ok);
    EXPECT_EQ(lp.size(), 1u);
}

TEST(BTreeLeafPageTest, RemoveAllEntries)
{
    auto lp = makePage();
    mustInsert(lp, 1, makeRid(0));
    mustInsert(lp, 2, makeRid(1));
    mustInsert(lp, 3, makeRid(2));
    mustRemove(lp, 2);
    mustRemove(lp, 1);
    mustRemove(lp, 3);
    EXPECT_TRUE(lp.isEmpty());
}

TEST(BTreeLeafPageTest, LookupAfterRemoveReturnsNullopt)
{
    auto lp = makePage();
    mustInsert(lp, 42, makeRid(0));
    mustRemove(lp, 42);
    EXPECT_FALSE(lp.lookup(42).has_value());
}

// ── Serialisation round-trip ──────────────────────────────────────────────────

TEST(BTreeLeafPageTest, SerializeEmptyPageSucceeds)
{
    auto lp = makePage(kPage1, kPage2);
    std::array<std::byte, kPageBodySize> buf{};
    EXPECT_EQ(lp.serialize(buf), Status::Ok);
}

TEST(BTreeLeafPageTest, DeserializeEmptyPageRoundTrip)
{
    auto lp = makePage(kPage1, kPage2);
    lp.setPrevPageId(5);
    lp.setNextPageId(7);

    std::array<std::byte, kPageBodySize> buf{};
    ASSERT_EQ(lp.serialize(buf), Status::Ok);

    BTreeLeafPage decoded;
    ASSERT_EQ(decoded.deserialize(buf), Status::Ok);

    EXPECT_EQ(decoded.size(),         0u);
    EXPECT_EQ(decoded.pageId(),       kPage1);
    EXPECT_EQ(decoded.parentPageId(), kPage2);
    EXPECT_EQ(decoded.prevPageId(),   5u);
    EXPECT_EQ(decoded.nextPageId(),   7u);
}

TEST(BTreeLeafPageTest, SerializeDeserializeWithEntries)
{
    auto lp = makePage(kPage2, kNoParent);
    mustInsert(lp, 100, RID{kPage1, 0});
    mustInsert(lp, 200, RID{kPage2, 1});
    mustInsert(lp, 300, RID{kPage3, 2});

    std::array<std::byte, kPageBodySize> buf{};
    ASSERT_EQ(lp.serialize(buf), Status::Ok);

    BTreeLeafPage decoded;
    ASSERT_EQ(decoded.deserialize(buf), Status::Ok);

    ASSERT_EQ(decoded.size(), 3u);
    EXPECT_EQ(decoded.keyAt(0), 100); EXPECT_EQ(decoded.valueAt(0), (RID{kPage1, 0}));
    EXPECT_EQ(decoded.keyAt(1), 200); EXPECT_EQ(decoded.valueAt(1), (RID{kPage2, 1}));
    EXPECT_EQ(decoded.keyAt(2), 300); EXPECT_EQ(decoded.valueAt(2), (RID{kPage3, 2}));
}

TEST(BTreeLeafPageTest, SerializeDeserializeNegativeKeys)
{
    auto lp = makePage();
    mustInsert(lp, -500, makeRid(0));
    mustInsert(lp, -100, makeRid(1));
    mustInsert(lp,    0, makeRid(2));

    std::array<std::byte, kPageBodySize> buf{};
    ASSERT_EQ(lp.serialize(buf), Status::Ok);

    BTreeLeafPage decoded;
    ASSERT_EQ(decoded.deserialize(buf), Status::Ok);

    ASSERT_EQ(decoded.size(), 3u);
    EXPECT_EQ(decoded.keyAt(0), -500);
    EXPECT_EQ(decoded.keyAt(1), -100);
    EXPECT_EQ(decoded.keyAt(2),    0);
}

TEST(BTreeLeafPageTest, SerializeDeserializeSiblingLinks)
{
    auto lp = makePage(kPage1, kPage2);
    lp.setPrevPageId(kPage3);
    lp.setNextPageId(42u);

    std::array<std::byte, kPageBodySize> buf{};
    ASSERT_EQ(lp.serialize(buf), Status::Ok);

    BTreeLeafPage decoded;
    ASSERT_EQ(decoded.deserialize(buf), Status::Ok);

    EXPECT_EQ(decoded.prevPageId(), kPage3);
    EXPECT_EQ(decoded.nextPageId(), 42u);
}

TEST(BTreeLeafPageTest, SerializeDeserializeInvalidSiblingLinks)
{
    auto lp = makePage();
    // Default: both sibling pointers are kInvalidPageId.

    std::array<std::byte, kPageBodySize> buf{};
    ASSERT_EQ(lp.serialize(buf), Status::Ok);

    BTreeLeafPage decoded;
    ASSERT_EQ(decoded.deserialize(buf), Status::Ok);

    EXPECT_EQ(decoded.prevPageId(), kInvalidPageId);
    EXPECT_EQ(decoded.nextPageId(), kInvalidPageId);
}

TEST(BTreeLeafPageTest, LookupWorksAfterDeserialize)
{
    auto lp = makePage();
    mustInsert(lp, 77, RID{kPage2, 3});

    std::array<std::byte, kPageBodySize> buf{};
    ASSERT_EQ(lp.serialize(buf), Status::Ok);

    BTreeLeafPage decoded;
    ASSERT_EQ(decoded.deserialize(buf), Status::Ok);

    auto result = decoded.lookup(77);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (RID{kPage2, 3}));
}

// ── Serialisation error handling ──────────────────────────────────────────────

TEST(BTreeLeafPageTest, SerializeFailsOnEmptyBuffer)
{
    auto lp = makePage();
    std::array<std::byte, 0> empty{};
    EXPECT_EQ(lp.serialize(empty), Status::IoError);
}

TEST(BTreeLeafPageTest, SerializeFailsOnBufferSmallerThanLeafHeader)
{
    auto lp = makePage();
    std::array<std::byte, BTreeLeafPage::kLeafHeaderSize - 1> small{};
    EXPECT_EQ(lp.serialize(small), Status::IoError);
}

TEST(BTreeLeafPageTest, SerializeFailsWhenBufferCannotFitAllEntries)
{
    auto lp = makePage();
    mustInsert(lp, 1, makeRid(0));
    mustInsert(lp, 2, makeRid(1));
    // Buffer just large enough for header but not two entries.
    std::array<std::byte, BTreeLeafPage::kLeafHeaderSize> small{};
    EXPECT_EQ(lp.serialize(small), Status::IoError);
}

// ── Deserialisation error handling ────────────────────────────────────────────

TEST(BTreeLeafPageTest, DeserializeFailsOnEmptyBuffer)
{
    BTreeLeafPage lp;
    std::array<std::byte, 0> empty{};
    EXPECT_EQ(lp.deserialize(empty), Status::IoError);
}

TEST(BTreeLeafPageTest, DeserializeFailsOnBufferSmallerThanLeafHeader)
{
    BTreeLeafPage lp;
    std::array<std::byte, BTreeLeafPage::kLeafHeaderSize - 1> small{};
    EXPECT_EQ(lp.deserialize(small), Status::IoError);
}

TEST(BTreeLeafPageTest, DeserializeDoesNotModifyFieldsOnFailure)
{
    auto lp = makePage(kPage2, kPage3);
    mustInsert(lp, 10, makeRid(0));
    lp.setPrevPageId(5);
    lp.setNextPageId(6);

    // Try to deserialise from a too-short buffer.
    std::array<std::byte, BTreeLeafPage::kLeafHeaderSize - 3> short_buf{};
    EXPECT_EQ(lp.deserialize(short_buf), Status::IoError);

    // Original state must be preserved.
    EXPECT_EQ(lp.size(),         1u);
    EXPECT_EQ(lp.pageId(),       kPage2);
    EXPECT_EQ(lp.parentPageId(), kPage3);
    EXPECT_EQ(lp.prevPageId(),   5u);
    EXPECT_EQ(lp.nextPageId(),   6u);
}

// ── Boundary conditions ───────────────────────────────────────────────────────

TEST(BTreeLeafPageTest, InsertMinInt64Key)
{
    auto lp = makePage();
    EXPECT_EQ(lp.insert(INT64_MIN, makeRid(0)), Status::Ok);
    EXPECT_EQ(lp.keyAt(0), INT64_MIN);
}

TEST(BTreeLeafPageTest, InsertMaxInt64Key)
{
    auto lp = makePage();
    EXPECT_EQ(lp.insert(INT64_MAX, makeRid(0)), Status::Ok);
    EXPECT_EQ(lp.keyAt(0), INT64_MAX);
}

TEST(BTreeLeafPageTest, InsertMinAndMaxInt64KeysSorted)
{
    auto lp = makePage();
    mustInsert(lp, INT64_MAX, makeRid(1));
    mustInsert(lp, INT64_MIN, makeRid(0));

    ASSERT_EQ(lp.size(), 2u);
    EXPECT_EQ(lp.keyAt(0), INT64_MIN);
    EXPECT_EQ(lp.keyAt(1), INT64_MAX);
}

TEST(BTreeLeafPageTest, LookupMinInt64Key)
{
    auto lp = makePage();
    mustInsert(lp, INT64_MIN, makeRid(0));
    EXPECT_TRUE(lp.lookup(INT64_MIN).has_value());
}

TEST(BTreeLeafPageTest, LookupMaxInt64Key)
{
    auto lp = makePage();
    mustInsert(lp, INT64_MAX, makeRid(0));
    EXPECT_TRUE(lp.lookup(INT64_MAX).has_value());
}

TEST(BTreeLeafPageTest, RemoveOnlyEntry)
{
    auto lp = makePage();
    mustInsert(lp, 1, makeRid(0));
    EXPECT_EQ(lp.remove(1), Status::Ok);
    EXPECT_TRUE(lp.isEmpty());
}

TEST(BTreeLeafPageTest, InsertAfterRemoveFromFullPage)
{
    auto lp = makePage();
    for (int64_t k = 0; k < static_cast<int64_t>(BTreeLeafPage::kMaxEntries); ++k)
    {
        mustInsert(lp, k, makeRid(static_cast<uint16_t>(k)));
    }
    ASSERT_TRUE(lp.isFull());
    mustRemove(lp, 0); // remove the first entry
    EXPECT_FALSE(lp.isFull());
    EXPECT_EQ(lp.insert(INT64_MAX, makeRid(0)), Status::Ok);
}

TEST(BTreeLeafPageTest, SerializeFullPageRoundTrip)
{
    auto lp = makePage(kPage3, kNoParent);
    for (int64_t k = 0; k < static_cast<int64_t>(BTreeLeafPage::kMaxEntries); ++k)
    {
        mustInsert(lp, k, RID{kPage1, static_cast<uint16_t>(k % 1000)});
    }
    ASSERT_TRUE(lp.isFull());

    std::array<std::byte, kPageBodySize> buf{};
    ASSERT_EQ(lp.serialize(buf), Status::Ok);

    BTreeLeafPage decoded;
    ASSERT_EQ(decoded.deserialize(buf), Status::Ok);

    ASSERT_EQ(decoded.size(), lp.size());
    for (uint16_t i = 0; i < decoded.size(); ++i)
    {
        EXPECT_EQ(decoded.keyAt(i), lp.keyAt(i)) << "mismatch at slot " << i;
        EXPECT_EQ(decoded.valueAt(i), lp.valueAt(i)) << "mismatch at slot " << i;
    }
}

} // namespace hamdb
