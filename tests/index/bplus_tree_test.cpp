#include "index/bplus_tree.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include "index/btree_internal_page.hpp"
#include "index/btree_leaf_page.hpp"
#include "storage/disk_manager.hpp"
#include <filesystem>
#include <gtest/gtest.h>

namespace hamdb
{

    class BPlusTreeTest : public ::testing::Test
    {
    protected:
        std::filesystem::path db_path_ = std::filesystem::temp_directory_path() / "bplus_tree_test.db";
        std::unique_ptr<DiskManager> dm_;
        std::unique_ptr<BufferPoolManager> bpm_;

        void SetUp() override
        {
            std::filesystem::remove(db_path_);
            dm_ = std::make_unique<DiskManager>(db_path_);
            ASSERT_EQ(dm_->createDatabase(), Status::Ok);
            ASSERT_EQ(dm_->openDatabase(), Status::Ok);
            bpm_ = std::make_unique<BufferPoolManager>(10, *dm_);
        }

        void TearDown() override
        {
            bpm_.reset();
            dm_.reset();
            std::filesystem::remove(db_path_);
        }
        
        // Helper to manually create a leaf page and fill it with some data
        PageId createLeafPage(const std::vector<std::pair<int64_t, RID>>& entries)
        {
            PageId page_id = kInvalidPageId;
            WritePageGuard guard;
            EXPECT_EQ(bpm_->newPageGuard(page_id, guard), Status::Ok);
            
            BTreeLeafPage leaf;
            leaf.init(page_id, kInvalidPageId);
            
            for (const auto& [k, v] : entries)
            {
                EXPECT_EQ(leaf.insert(k, v), Status::Ok);
            }
            
            EXPECT_EQ(leaf.serialize(guard.pageMut().body()), Status::Ok);
            guard.markDirty();
            return page_id;
        }

        // Helper to manually create an internal page
        PageId createInternalPage(PageId left_child, int64_t key, PageId right_child)
        {
            PageId page_id = kInvalidPageId;
            WritePageGuard guard;
            EXPECT_EQ(bpm_->newPageGuard(page_id, guard), Status::Ok);
            
            BTreeInternalPage internal;
            internal.init(page_id, kInvalidPageId);
            internal.populateNewRoot(left_child, key, right_child);
            
            EXPECT_EQ(internal.serialize(guard.pageMut().body()), Status::Ok);
            guard.markDirty();
            return page_id;
        }
    };

    TEST_F(BPlusTreeTest, EmptyTree)
    {
        BPlusTree tree(*bpm_);
        tree.create();
        
        EXPECT_TRUE(tree.isEmpty());
        EXPECT_EQ(tree.getValue(100), std::nullopt);
    }

    TEST_F(BPlusTreeTest, SingleLeafLookup)
    {
        PageId leaf_id = createLeafPage({
            {10, RID{1, 0}},
            {20, RID{1, 1}},
            {30, RID{1, 2}}
        });
        
        BPlusTree tree(*bpm_);
        tree.open(leaf_id);
        
        EXPECT_FALSE(tree.isEmpty());
        
        auto res1 = tree.getValue(10);
        ASSERT_TRUE(res1.has_value());
        EXPECT_EQ(res1->getPageId(), 1);
        EXPECT_EQ(res1->getSlotId(), 0);

        auto res2 = tree.getValue(20);
        ASSERT_TRUE(res2.has_value());
        EXPECT_EQ(res2->getPageId(), 1);
        EXPECT_EQ(res2->getSlotId(), 1);

        auto res3 = tree.getValue(30);
        ASSERT_TRUE(res3.has_value());
        EXPECT_EQ(res3->getPageId(), 1);
        EXPECT_EQ(res3->getSlotId(), 2);
        
        // Missing keys
        EXPECT_EQ(tree.getValue(5), std::nullopt);
        EXPECT_EQ(tree.getValue(25), std::nullopt);
        EXPECT_EQ(tree.getValue(50), std::nullopt);
    }

    TEST_F(BPlusTreeTest, MultiLevelRouting)
    {
        // Leaf 1: keys 1, 2, 3
        PageId leaf1 = createLeafPage({
            {1, RID{2, 0}},
            {2, RID{2, 1}},
            {3, RID{2, 2}}
        });
        
        // Leaf 2: keys 5, 6, 7
        PageId leaf2 = createLeafPage({
            {5, RID{3, 0}},
            {6, RID{3, 1}},
            {7, RID{3, 2}}
        });
        
        // Leaf 3: keys 10, 11
        PageId leaf3 = createLeafPage({
            {10, RID{4, 0}},
            {11, RID{4, 1}}
        });
        
        // Internal 1: Routes between Leaf 1 and Leaf 2 (separator = 5)
        PageId int1 = createInternalPage(leaf1, 5, leaf2);
        
        // Internal 2: Root (separator = 10, left = int1, right = leaf3)
        // Note: For simplicity of test, right child is just a leaf directly (unbalanced),
        // or we could make another internal node. BPlusTree search doesn't care about balance.
        PageId root = createInternalPage(int1, 10, leaf3);
        
        BPlusTree tree(*bpm_);
        tree.open(root);
        
        // Boundary and inner key lookups
        auto res_left = tree.getValue(1);
        ASSERT_TRUE(res_left.has_value());
        EXPECT_EQ(res_left->getPageId(), 2);
        
        auto res_mid = tree.getValue(6);
        ASSERT_TRUE(res_mid.has_value());
        EXPECT_EQ(res_mid->getPageId(), 3);
        
        auto res_right = tree.getValue(11);
        ASSERT_TRUE(res_right.has_value());
        EXPECT_EQ(res_right->getPageId(), 4);
        
        // Missing keys
        EXPECT_EQ(tree.getValue(0), std::nullopt);
        EXPECT_EQ(tree.getValue(4), std::nullopt);
        EXPECT_EQ(tree.getValue(8), std::nullopt);
        EXPECT_EQ(tree.getValue(20), std::nullopt);
    }
    
    TEST_F(BPlusTreeTest, AutomaticGuardRelease)
    {
        PageId root_id = createLeafPage({ {10, RID{1, 0}} });
        BPlusTree tree(*bpm_);
        tree.open(root_id);
        
        // Call it many times. If guards are not released, the buffer pool (size 10) will fill up and stall.
        for (int i = 0; i < 100; ++i)
        {
            auto res = tree.getValue(10);
            ASSERT_TRUE(res.has_value());
        }
    }

} // namespace hamdb
