#include "buffer/buffer_pool_manager.hpp"
#include "index/bplus_tree.hpp"
#include "index/bplus_tree_iterator.hpp"
#include "storage/disk_manager.hpp"
#include <filesystem>
#include <gtest/gtest.h>

namespace hamdb
{
    class BPlusTreeIteratorTest : public ::testing::Test
    {
    protected:
        std::filesystem::path db_path_ =
            std::filesystem::temp_directory_path() / "bplus_tree_iter_test.db";
        std::unique_ptr<DiskManager> dm_;
        std::unique_ptr<BufferPoolManager> bpm_;

        void SetUp() override
        {
            std::filesystem::remove(db_path_);
            dm_ = std::make_unique<DiskManager>(db_path_);
            ASSERT_EQ(dm_->createDatabase(), Status::Ok);
            ASSERT_EQ(dm_->openDatabase(), Status::Ok);
            bpm_ = std::make_unique<BufferPoolManager>(50, *dm_);
        }

        void TearDown() override
        {
            bpm_.reset();
            dm_.reset();
            std::filesystem::remove(db_path_);
        }
    };

    TEST_F(BPlusTreeIteratorTest, EmptyTree)
    {
        BPlusTree tree(*bpm_);
        tree.create();

        auto it = tree.begin();
        EXPECT_TRUE(it.isEnd());
        EXPECT_TRUE(it == tree.end());
    }

    TEST_F(BPlusTreeIteratorTest, SingleNodeScan)
    {
        BPlusTree tree(*bpm_);
        tree.create();

        for (int i = 1; i <= 5; ++i)
        {
            EXPECT_EQ(tree.insert(i, RID{1, static_cast<uint16_t>(i)}), Status::Ok);
        }

        auto it = tree.begin();
        int expected_key = 1;
        while (!it.isEnd())
        {
            auto [key, rid] = *it;
            EXPECT_EQ(key, expected_key);
            EXPECT_EQ(rid.getSlotId(), expected_key);
            ++expected_key;
            ++it;
        }
        EXPECT_EQ(expected_key, 6);
    }

    TEST_F(BPlusTreeIteratorTest, MultiNodeScan)
    {
        BPlusTree tree(*bpm_);
        tree.create();

        // Insert enough to trigger splits
        for (int i = 1; i <= 1000; ++i)
        {
            EXPECT_EQ(tree.insert(i, RID{1, static_cast<uint16_t>(i)}), Status::Ok);
        }

        auto it = tree.begin();
        int expected_key = 1;
        while (!it.isEnd())
        {
            auto [key, rid] = *it;
            EXPECT_EQ(key, expected_key);
            EXPECT_EQ(rid.getSlotId(), expected_key);
            ++expected_key;
            ++it;
        }
        EXPECT_EQ(expected_key, 1001);
    }

    TEST_F(BPlusTreeIteratorTest, RangeScanLowerBound)
    {
        BPlusTree tree(*bpm_);
        tree.create();

        for (int i = 10; i <= 100; i += 10) // 10, 20, 30...
        {
            EXPECT_EQ(tree.insert(i, RID{1, static_cast<uint16_t>(i)}), Status::Ok);
        }

        // Exact match
        auto it = tree.begin(50);
        ASSERT_FALSE(it.isEnd());
        EXPECT_EQ((*it).first, 50);

        // Lower bound
        auto it2 = tree.begin(55);
        ASSERT_FALSE(it2.isEnd());
        EXPECT_EQ((*it2).first, 60);

        // Out of bounds
        auto it3 = tree.begin(150);
        EXPECT_TRUE(it3.isEnd());

        // Range scan
        auto it4 = tree.begin(30);
        int expected_key = 30;
        while (expected_key <= 60 && !it4.isEnd())
        {
            EXPECT_EQ((*it4).first, expected_key);
            expected_key += 10;
            ++it4;
        }
    }
} // namespace hamdb
