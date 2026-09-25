/// @file btree_page_test.cpp
/// @brief Unit tests for BTreePage — the shared B+ Tree node header.

#include "index/btree_page.hpp"

#include "common/constants.hpp"
#include "common/enums.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

namespace hamdb
{

    // ── Fixture ───────────────────────────────────────────────────────────────────

    /// Helper constants used throughout all test cases.
    namespace
    {
        constexpr PageId kTestPageId = 42;
        constexpr PageId kTestParentId = 7;
        constexpr uint16_t kTestMaxSize = 128;
    } // anonymous namespace

    // ── Default initialisation ────────────────────────────────────────────────────

    TEST(BTreePageTest, DefaultConstructedPageTypeIsLeaf)
    {
        BTreePage page;
        EXPECT_EQ(page.pageType(), PageType::BTreeLeaf);
    }

    TEST(BTreePageTest, DefaultConstructedCurrentSizeIsZero)
    {
        BTreePage page;
        EXPECT_EQ(page.currentSize(), 0u);
    }

    TEST(BTreePageTest, DefaultConstructedMaxSizeIsZero)
    {
        BTreePage page;
        EXPECT_EQ(page.maxSize(), 0u);
    }

    TEST(BTreePageTest, DefaultConstructedPageIdIsInvalid)
    {
        BTreePage page;
        EXPECT_EQ(page.pageId(), kInvalidPageId);
    }

    TEST(BTreePageTest, DefaultConstructedParentPageIdIsInvalid)
    {
        BTreePage page;
        EXPECT_EQ(page.parentPageId(), kInvalidPageId);
    }

    TEST(BTreePageTest, DefaultConstructedIsRoot)
    {
        // A default-constructed page has kInvalidPageId as parent → isRoot() true.
        BTreePage page;
        EXPECT_TRUE(page.isRoot());
    }

    TEST(BTreePageTest, DefaultConstructedIsFullWhenMaxSizeIsZero)
    {
        // currentSize (0) >= maxSize (0) → full.
        BTreePage page;
        EXPECT_TRUE(page.isFull());
    }

    // ── Parameterised construction ─────────────────────────────────────────────────

    TEST(BTreePageTest, ParameterisedConstructorSetsLeafType)
    {
        BTreePage page(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        EXPECT_EQ(page.pageType(), PageType::BTreeLeaf);
    }

    TEST(BTreePageTest, ParameterisedConstructorSetsInternalType)
    {
        BTreePage page(PageType::BTreeInternal, kTestPageId, kTestParentId, kTestMaxSize);
        EXPECT_EQ(page.pageType(), PageType::BTreeInternal);
    }

    TEST(BTreePageTest, ParameterisedConstructorSetsPageId)
    {
        BTreePage page(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        EXPECT_EQ(page.pageId(), kTestPageId);
    }

    TEST(BTreePageTest, ParameterisedConstructorSetsParentPageId)
    {
        BTreePage page(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        EXPECT_EQ(page.parentPageId(), kTestParentId);
    }

    TEST(BTreePageTest, ParameterisedConstructorSetsMaxSize)
    {
        BTreePage page(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        EXPECT_EQ(page.maxSize(), kTestMaxSize);
    }

    TEST(BTreePageTest, ParameterisedConstructorCurrentSizeIsZero)
    {
        BTreePage page(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        EXPECT_EQ(page.currentSize(), 0u);
    }

    // ── Getters and Setters ───────────────────────────────────────────────────────

    TEST(BTreePageTest, SetPageTypeLeaf)
    {
        BTreePage page;
        page.setPageType(PageType::BTreeLeaf);
        EXPECT_EQ(page.pageType(), PageType::BTreeLeaf);
    }

    TEST(BTreePageTest, SetPageTypeInternal)
    {
        BTreePage page;
        page.setPageType(PageType::BTreeInternal);
        EXPECT_EQ(page.pageType(), PageType::BTreeInternal);
    }

    TEST(BTreePageTest, SetCurrentSize)
    {
        BTreePage page(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        page.setCurrentSize(64);
        EXPECT_EQ(page.currentSize(), 64u);
    }

    TEST(BTreePageTest, SetMaxSize)
    {
        BTreePage page;
        page.setMaxSize(256);
        EXPECT_EQ(page.maxSize(), 256u);
    }

    TEST(BTreePageTest, SetPageId)
    {
        BTreePage page;
        page.setPageId(99);
        EXPECT_EQ(page.pageId(), 99u);
    }

    TEST(BTreePageTest, SetParentPageId)
    {
        BTreePage page;
        page.setParentPageId(3);
        EXPECT_EQ(page.parentPageId(), 3u);
    }

    // ── isRoot ────────────────────────────────────────────────────────────────────

    TEST(BTreePageTest, IsRootWhenParentIsInvalid)
    {
        BTreePage page(PageType::BTreeLeaf, kTestPageId, kInvalidPageId, kTestMaxSize);
        EXPECT_TRUE(page.isRoot());
    }

    TEST(BTreePageTest, IsNotRootWhenParentIsValid)
    {
        BTreePage page(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        EXPECT_FALSE(page.isRoot());
    }

    TEST(BTreePageTest, SetParentToInvalidMakesRoot)
    {
        BTreePage page(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        EXPECT_FALSE(page.isRoot());
        page.setParentPageId(kInvalidPageId);
        EXPECT_TRUE(page.isRoot());
    }

    TEST(BTreePageTest, SetParentToValidRemovesRoot)
    {
        BTreePage page(PageType::BTreeLeaf, kTestPageId, kInvalidPageId, kTestMaxSize);
        EXPECT_TRUE(page.isRoot());
        page.setParentPageId(5);
        EXPECT_FALSE(page.isRoot());
    }

    // ── isFull ────────────────────────────────────────────────────────────────────

    TEST(BTreePageTest, IsNotFullWhenCurrentSizeLessThanMax)
    {
        BTreePage page(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        page.setCurrentSize(kTestMaxSize - 1);
        EXPECT_FALSE(page.isFull());
    }

    TEST(BTreePageTest, IsFullWhenCurrentSizeEqualsMax)
    {
        BTreePage page(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        page.setCurrentSize(kTestMaxSize);
        EXPECT_TRUE(page.isFull());
    }

    TEST(BTreePageTest, IsFullWhenCurrentSizeExceedsMax)
    {
        BTreePage page(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        // Overflow guard: max_size is uint16 so set current larger via setter.
        page.setMaxSize(10);
        page.setCurrentSize(11);
        EXPECT_TRUE(page.isFull());
    }

    // ── Header size ───────────────────────────────────────────────────────────────

    TEST(BTreePageTest, HeaderSizeIs16Bytes)
    {
        EXPECT_EQ(BTreePage::kHeaderSize, 16u);
    }

    // ── Serialised layout ─────────────────────────────────────────────────────────

    TEST(BTreePageTest, SerializeLeafPageTypeEncodedAtByte0)
    {
        BTreePage page(PageType::BTreeLeaf, 0, kInvalidPageId, 0);
        std::array<std::byte, BTreePage::kHeaderSize> buf{};
        ASSERT_EQ(page.serialize(buf), Status::Ok);
        // Byte 0 must equal the numeric value of BTreeLeaf (6).
        EXPECT_EQ(static_cast<uint8_t>(buf[0]), static_cast<uint8_t>(PageType::BTreeLeaf));
    }

    TEST(BTreePageTest, SerializeInternalPageTypeEncodedAtByte0)
    {
        BTreePage page(PageType::BTreeInternal, 0, kInvalidPageId, 0);
        std::array<std::byte, BTreePage::kHeaderSize> buf{};
        ASSERT_EQ(page.serialize(buf), Status::Ok);
        EXPECT_EQ(static_cast<uint8_t>(buf[0]), static_cast<uint8_t>(PageType::BTreeInternal));
    }

    TEST(BTreePageTest, SerializeCurrentSizeLittleEndianAtBytes1And2)
    {
        BTreePage page(PageType::BTreeLeaf, 0, kInvalidPageId, 0);
        page.setCurrentSize(0x0102);
        std::array<std::byte, BTreePage::kHeaderSize> buf{};
        ASSERT_EQ(page.serialize(buf), Status::Ok);
        // Little-endian: low byte first.
        EXPECT_EQ(static_cast<uint8_t>(buf[1]), 0x02u);
        EXPECT_EQ(static_cast<uint8_t>(buf[2]), 0x01u);
    }

    TEST(BTreePageTest, SerializeMaxSizeLittleEndianAtBytes3And4)
    {
        BTreePage page(PageType::BTreeLeaf, 0, kInvalidPageId, 0x0304);
        std::array<std::byte, BTreePage::kHeaderSize> buf{};
        ASSERT_EQ(page.serialize(buf), Status::Ok);
        EXPECT_EQ(static_cast<uint8_t>(buf[3]), 0x04u);
        EXPECT_EQ(static_cast<uint8_t>(buf[4]), 0x03u);
    }

    TEST(BTreePageTest, SerializeParentPageIdLittleEndianAtBytes5To8)
    {
        BTreePage page(PageType::BTreeLeaf, 0, 0x01020304u, 0);
        std::array<std::byte, BTreePage::kHeaderSize> buf{};
        ASSERT_EQ(page.serialize(buf), Status::Ok);
        EXPECT_EQ(static_cast<uint8_t>(buf[5]), 0x04u);
        EXPECT_EQ(static_cast<uint8_t>(buf[6]), 0x03u);
        EXPECT_EQ(static_cast<uint8_t>(buf[7]), 0x02u);
        EXPECT_EQ(static_cast<uint8_t>(buf[8]), 0x01u);
    }

    TEST(BTreePageTest, SerializePageIdLittleEndianAtBytes9To12)
    {
        BTreePage page(PageType::BTreeLeaf, 0xDEADBEEFu, kInvalidPageId, 0);
        std::array<std::byte, BTreePage::kHeaderSize> buf{};
        ASSERT_EQ(page.serialize(buf), Status::Ok);
        EXPECT_EQ(static_cast<uint8_t>(buf[9]), 0xEFu);
        EXPECT_EQ(static_cast<uint8_t>(buf[10]), 0xBEu);
        EXPECT_EQ(static_cast<uint8_t>(buf[11]), 0xADu);
        EXPECT_EQ(static_cast<uint8_t>(buf[12]), 0xDEu);
    }

    TEST(BTreePageTest, SerializeReservedBytesAreZeroAtBytes13To15)
    {
        BTreePage page(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        std::array<std::byte, BTreePage::kHeaderSize> buf{};
        buf.fill(std::byte{0xFF}); // Pre-fill with non-zero to verify zero-fill.
        ASSERT_EQ(page.serialize(buf), Status::Ok);
        EXPECT_EQ(static_cast<uint8_t>(buf[13]), 0x00u);
        EXPECT_EQ(static_cast<uint8_t>(buf[14]), 0x00u);
        EXPECT_EQ(static_cast<uint8_t>(buf[15]), 0x00u);
    }

    TEST(BTreePageTest, SerializeFailsIfBufferTooSmall)
    {
        BTreePage page(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        std::array<std::byte, BTreePage::kHeaderSize - 1> small_buf{};
        EXPECT_EQ(page.serialize(small_buf), Status::IoError);
    }

    // ── Deserialise ───────────────────────────────────────────────────────────────

    TEST(BTreePageTest, DeserializeRoundTripLeafPage)
    {
        BTreePage original(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        original.setCurrentSize(55);

        std::array<std::byte, BTreePage::kHeaderSize> buf{};
        ASSERT_EQ(original.serialize(buf), Status::Ok);

        BTreePage decoded;
        ASSERT_EQ(decoded.deserialize(buf), Status::Ok);

        EXPECT_EQ(decoded.pageType(), PageType::BTreeLeaf);
        EXPECT_EQ(decoded.currentSize(), 55u);
        EXPECT_EQ(decoded.maxSize(), kTestMaxSize);
        EXPECT_EQ(decoded.parentPageId(), kTestParentId);
        EXPECT_EQ(decoded.pageId(), kTestPageId);
    }

    TEST(BTreePageTest, DeserializeRoundTripInternalPage)
    {
        BTreePage original(PageType::BTreeInternal, 1, kInvalidPageId, 64);
        original.setCurrentSize(33);

        std::array<std::byte, BTreePage::kHeaderSize> buf{};
        ASSERT_EQ(original.serialize(buf), Status::Ok);

        BTreePage decoded;
        ASSERT_EQ(decoded.deserialize(buf), Status::Ok);

        EXPECT_EQ(decoded.pageType(), PageType::BTreeInternal);
        EXPECT_EQ(decoded.currentSize(), 33u);
        EXPECT_EQ(decoded.maxSize(), 64u);
        EXPECT_EQ(decoded.pageId(), 1u);
        EXPECT_TRUE(decoded.isRoot());
    }

    TEST(BTreePageTest, DeserializeFailsIfBufferTooSmall)
    {
        std::array<std::byte, BTreePage::kHeaderSize - 1> small_buf{};
        BTreePage page;
        EXPECT_EQ(page.deserialize(small_buf), Status::IoError);
    }

    TEST(BTreePageTest, DeserializeDoesNotModifyFieldsOnFailure)
    {
        BTreePage page(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        page.setCurrentSize(77);

        std::array<std::byte, BTreePage::kHeaderSize - 2> small_buf{};
        // Must fail.
        EXPECT_EQ(page.deserialize(small_buf), Status::IoError);

        // All original fields must remain unchanged.
        EXPECT_EQ(page.pageType(), PageType::BTreeLeaf);
        EXPECT_EQ(page.currentSize(), 77u);
        EXPECT_EQ(page.maxSize(), kTestMaxSize);
        EXPECT_EQ(page.parentPageId(), kTestParentId);
        EXPECT_EQ(page.pageId(), kTestPageId);
    }

    // ── Equality ──────────────────────────────────────────────────────────────────

    TEST(BTreePageTest, EqualityIdenticalPages)
    {
        BTreePage a(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        BTreePage b(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        EXPECT_TRUE(a == b);
        EXPECT_FALSE(a != b);
    }

    TEST(BTreePageTest, InequalityDifferentPageType)
    {
        BTreePage a(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        BTreePage b(PageType::BTreeInternal, kTestPageId, kTestParentId, kTestMaxSize);
        EXPECT_FALSE(a == b);
        EXPECT_TRUE(a != b);
    }

    TEST(BTreePageTest, InequalityDifferentCurrentSize)
    {
        BTreePage a(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        BTreePage b(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        b.setCurrentSize(1);
        EXPECT_NE(a, b);
    }

    TEST(BTreePageTest, InequalityDifferentMaxSize)
    {
        BTreePage a(PageType::BTreeLeaf, kTestPageId, kTestParentId, 10);
        BTreePage b(PageType::BTreeLeaf, kTestPageId, kTestParentId, 20);
        EXPECT_NE(a, b);
    }

    TEST(BTreePageTest, InequalityDifferentParentPageId)
    {
        BTreePage a(PageType::BTreeLeaf, kTestPageId, 1, kTestMaxSize);
        BTreePage b(PageType::BTreeLeaf, kTestPageId, 2, kTestMaxSize);
        EXPECT_NE(a, b);
    }

    TEST(BTreePageTest, InequalityDifferentPageId)
    {
        BTreePage a(PageType::BTreeLeaf, 1, kTestParentId, kTestMaxSize);
        BTreePage b(PageType::BTreeLeaf, 2, kTestParentId, kTestMaxSize);
        EXPECT_NE(a, b);
    }

    // ── Parent metadata ───────────────────────────────────────────────────────────

    TEST(BTreePageTest, RootPageHasInvalidParent)
    {
        BTreePage root(PageType::BTreeLeaf, kTestPageId, kInvalidPageId, kTestMaxSize);
        EXPECT_EQ(root.parentPageId(), kInvalidPageId);
        EXPECT_TRUE(root.isRoot());
    }

    TEST(BTreePageTest, NonRootPageHasValidParent)
    {
        BTreePage child(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        EXPECT_EQ(child.parentPageId(), kTestParentId);
        EXPECT_FALSE(child.isRoot());
    }

    TEST(BTreePageTest, ParentPageIdRoundTripThroughSerialization)
    {
        constexpr PageId kParent = 12345;
        BTreePage page(PageType::BTreeLeaf, kTestPageId, kParent, kTestMaxSize);

        std::array<std::byte, BTreePage::kHeaderSize> buf{};
        ASSERT_EQ(page.serialize(buf), Status::Ok);

        BTreePage decoded;
        ASSERT_EQ(decoded.deserialize(buf), Status::Ok);
        EXPECT_EQ(decoded.parentPageId(), kParent);
        EXPECT_FALSE(decoded.isRoot());
    }

    TEST(BTreePageTest, InvalidParentRoundTripThroughSerialization)
    {
        BTreePage root(PageType::BTreeLeaf, kTestPageId, kInvalidPageId, kTestMaxSize);

        std::array<std::byte, BTreePage::kHeaderSize> buf{};
        ASSERT_EQ(root.serialize(buf), Status::Ok);

        BTreePage decoded;
        ASSERT_EQ(decoded.deserialize(buf), Status::Ok);
        EXPECT_EQ(decoded.parentPageId(), kInvalidPageId);
        EXPECT_TRUE(decoded.isRoot());
    }

    // ── Large buffer compatibility ─────────────────────────────────────────────────

    TEST(BTreePageTest, SerializeIntoLargerBufferSucceeds)
    {
        BTreePage page(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        std::array<std::byte, 64> large_buf{};
        // Serialize writes only first kHeaderSize bytes; rest must remain untouched.
        large_buf.fill(std::byte{0xAB});
        EXPECT_EQ(page.serialize(large_buf), Status::Ok);
        // Bytes beyond kHeaderSize should not have been touched.
        for (std::size_t i = BTreePage::kHeaderSize; i < large_buf.size(); ++i)
        {
            EXPECT_EQ(large_buf[i], std::byte{0xAB}) << "byte " << i << " was modified";
        }
    }

    TEST(BTreePageTest, DeserializeFromLargerBufferSucceeds)
    {
        BTreePage original(PageType::BTreeLeaf, kTestPageId, kTestParentId, kTestMaxSize);
        std::array<std::byte, 64> large_buf{};
        ASSERT_EQ(original.serialize(large_buf), Status::Ok);

        BTreePage decoded;
        // Passing a span larger than kHeaderSize — only the first 16 bytes matter.
        ASSERT_EQ(decoded.deserialize(large_buf), Status::Ok);
        EXPECT_EQ(decoded, original);
    }

} // namespace hamdb
