#include "common/enums.hpp"
#include "storage/page_header.hpp"
#include <gtest/gtest.h>

namespace hamdb
{

    // ── PageHeader default construction ───────────────────────────────────────────

    TEST(PageHeaderTest, DefaultConstructionYieldsFreeType)
    {
        PageHeader h;
        EXPECT_EQ(h.page_id, kInvalidPageId);
        EXPECT_EQ(h.page_type, PageType::Free);
    }

    TEST(PageHeaderTest, ParameterizedConstructionSetsFields)
    {
        PageHeader h(7, PageType::Table);
        EXPECT_EQ(h.page_id, 7u);
        EXPECT_EQ(h.page_type, PageType::Table);
    }

    TEST(PageHeaderTest, EqualityOperator)
    {
        PageHeader a(1, PageType::Table);
        PageHeader b(1, PageType::Table);
        EXPECT_EQ(a, b);
    }

    TEST(PageHeaderTest, InequalityOperator)
    {
        PageHeader a(1, PageType::Table);
        PageHeader b(2, PageType::Table);
        EXPECT_NE(a, b);
    }

    TEST(PageHeaderTest, SizeConstantIs24Bytes)
    {
        EXPECT_EQ(PageHeader::kSize, 24u);
    }

} // namespace hamdb
