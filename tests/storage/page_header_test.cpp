#include <gtest/gtest.h>
#include "storage/page_header.h"

namespace hamdb {

// ── PageHeader default construction ───────────────────────────────────────────

TEST(PageHeaderTest, DefaultConstructionYieldsInvalidState) {
    PageHeader h;
    EXPECT_EQ(h.page_id,   kInvalidPageId);
    EXPECT_EQ(h.page_type, PageHeader::PageType::Invalid);
}

TEST(PageHeaderTest, ParameterizedConstructionSetsFields) {
    PageHeader h(7, PageHeader::PageType::Data);
    EXPECT_EQ(h.page_id,   7u);
    EXPECT_EQ(h.page_type, PageHeader::PageType::Data);
}

TEST(PageHeaderTest, EqualityOperator) {
    PageHeader a(1, PageHeader::PageType::Data);
    PageHeader b(1, PageHeader::PageType::Data);
    EXPECT_EQ(a, b);
}

TEST(PageHeaderTest, InequalityOperator) {
    PageHeader a(1, PageHeader::PageType::Data);
    PageHeader b(2, PageHeader::PageType::Data);
    EXPECT_NE(a, b);
}

TEST(PageHeaderTest, SizeConstantIs16Bytes) {
    EXPECT_EQ(PageHeader::kSize, 16u);
}

} // namespace hamdb
