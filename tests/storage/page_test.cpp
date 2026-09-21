#include <gtest/gtest.h>
#include "storage/page.h"
#include "storage/page_header.h"

namespace hamdb {

// ── Page construction ─────────────────────────────────────────────────────────

TEST(PageTest, DefaultConstructionProducesInvalidId) {
    Page p;
    EXPECT_EQ(p.id(), kInvalidPageId);
}

TEST(PageTest, HeaderConstructorSetsId) {
    PageHeader hdr(42, PageHeader::PageType::Data);
    Page p(hdr);
    EXPECT_EQ(p.id(), 42u);
}

TEST(PageTest, TotalSizeMatchesConstant) {
    Page p;
    EXPECT_EQ(p.data().size(), Page::kSize);
}

TEST(PageTest, BodySizeIsPageSizeMinusHeaderSize) {
    Page p;
    EXPECT_EQ(p.body().size(), Page::kBodySize);
    EXPECT_EQ(Page::kBodySize, Page::kSize - PageHeader::kSize);
}

TEST(PageTest, ClearZeroesAllBytes) {
    PageHeader hdr(5, PageHeader::PageType::Meta);
    Page p(hdr);
    p.clear();
    EXPECT_EQ(p.id(), kInvalidPageId);
    for (auto byte : p.data()) {
        EXPECT_EQ(byte, std::byte{0});
    }
}

TEST(PageTest, NonCopyable) {
    EXPECT_FALSE(std::is_copy_constructible_v<Page>);
    EXPECT_FALSE(std::is_copy_assignable_v<Page>);
}

TEST(PageTest, Movable) {
    EXPECT_TRUE(std::is_move_constructible_v<Page>);
    EXPECT_TRUE(std::is_move_assignable_v<Page>);
}

} // namespace hamdb
