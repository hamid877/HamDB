#include "buffer/lruk_replacer.hpp"
#include <gtest/gtest.h>
#include <stdexcept>

namespace hamdb
{

    TEST(LRUKReplacerTest, BasicEviction)
    {
        LRUKReplacer replacer(7, 2);

        // Add six elements to the replacer. We have [1,2,3,4,5,6].
        replacer.recordAccess(1);
        replacer.recordAccess(2);
        replacer.recordAccess(3);
        replacer.recordAccess(4);
        replacer.recordAccess(5);
        replacer.recordAccess(6);
        
        replacer.setEvictable(1, true);
        replacer.setEvictable(2, true);
        replacer.setEvictable(3, true);
        replacer.setEvictable(4, true);
        replacer.setEvictable(5, true);
        replacer.setEvictable(6, false); // Frame 6 is pinned
        
        EXPECT_EQ(5, replacer.size());

        // Insert access history for frame 1. Now frame 1 has two accesses total.
        // All other frames have one access.
        replacer.recordAccess(1);

        // Evict three pages from the replacer. Elements with one access (< k) have +inf distance.
        // Ties are broken by oldest access timestamp (FIFO).
        // The order of elements with one access is 2, 3, 4, 5.
        // Therefore, 2, 3, 4 should be evicted in order.
        FrameId victim;
        EXPECT_TRUE(replacer.evict(victim));
        EXPECT_EQ(2, victim);
        replacer.remove(victim);

        EXPECT_TRUE(replacer.evict(victim));
        EXPECT_EQ(3, victim);
        replacer.remove(victim);

        EXPECT_TRUE(replacer.evict(victim));
        EXPECT_EQ(4, victim);
        replacer.remove(victim);
        EXPECT_EQ(2, replacer.size());

        // Now replacer has frames 1 and 5.
        // Frame 1 has 2 accesses (distance is finite).
        // Frame 5 has 1 access (distance is +inf).
        // Frame 5 should be evicted first.
        EXPECT_TRUE(replacer.evict(victim));
        EXPECT_EQ(5, victim);
        replacer.remove(victim);
        EXPECT_EQ(1, replacer.size());

        // Frame 1 should be evicted next.
        EXPECT_TRUE(replacer.evict(victim));
        EXPECT_EQ(1, victim);
        replacer.remove(victim);
        EXPECT_EQ(0, replacer.size());

        // Nothing left to evict.
        EXPECT_FALSE(replacer.evict(victim));
    }

    TEST(LRUKReplacerTest, PinnedFramesAreNotEvicted)
    {
        LRUKReplacer replacer(3, 2);
        
        replacer.recordAccess(1);
        replacer.recordAccess(2);
        replacer.setEvictable(1, true);
        // Frame 2 is pinned (evictable = false by default)
        
        FrameId victim;
        EXPECT_TRUE(replacer.evict(victim));
        EXPECT_EQ(1, victim);
        replacer.remove(victim);
        
        EXPECT_FALSE(replacer.evict(victim));
    }

    TEST(LRUKReplacerTest, HistoryTrimming)
    {
        LRUKReplacer replacer(3, 2);
        
        // Access frame 1 three times. First access will be pushed out.
        replacer.recordAccess(1); // ts 1
        replacer.recordAccess(1); // ts 2
        replacer.recordAccess(1); // ts 3
        replacer.setEvictable(1, true);

        // Access frame 2 two times. First access is later than frame 1's second access.
        replacer.recordAccess(2); // ts 4
        replacer.recordAccess(2); // ts 5
        replacer.setEvictable(2, true);

        // Frame 1 history: [2, 3]. Earliest is 2.
        // Frame 2 history: [4, 5]. Earliest is 4.
        // Both have K=2 accesses (finite distance).
        // Largest backward K-distance means oldest K-th access.
        // Frame 1's K-th access is ts 2. Frame 2's is ts 4.
        // Frame 1 has larger distance, so it should be evicted first.
        FrameId victim;
        EXPECT_TRUE(replacer.evict(victim));
        EXPECT_EQ(1, victim);
    }
    
    TEST(LRUKReplacerTest, RemoveNonEvictableThrows)
    {
        LRUKReplacer replacer(3, 2);
        replacer.recordAccess(1);
        EXPECT_THROW(replacer.remove(1), std::invalid_argument);
    }

} // namespace hamdb
