#include "storage/rid.hpp"
#include <gtest/gtest.h>

namespace hamdb
{

    class RIDTest : public ::testing::Test
    {
    };

    TEST_F(RIDTest, DefaultConstructIsInvalid)
    {
        RID rid;
        EXPECT_EQ(rid.getPageId(), kInvalidPageId);
        EXPECT_EQ(rid.getSlotId(), 0);
        EXPECT_FALSE(rid.isValid());
    }

    TEST_F(RIDTest, ParamConstructIsAssigned)
    {
        RID rid(42, 7);
        EXPECT_EQ(rid.getPageId(), 42);
        EXPECT_EQ(rid.getSlotId(), 7);
        EXPECT_TRUE(rid.isValid());
    }

    TEST_F(RIDTest, EqualityOperators)
    {
        RID r1(10, 5);
        RID r2(10, 5);
        RID r3(10, 6);
        RID r4(11, 5);

        EXPECT_TRUE(r1 == r2);
        EXPECT_FALSE(r1 != r2);

        EXPECT_TRUE(r1 != r3);
        EXPECT_TRUE(r1 != r4);
    }

} // namespace hamdb
