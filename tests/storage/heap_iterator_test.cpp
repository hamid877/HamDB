#include "storage/table_heap.hpp"
#include <filesystem>
#include <gtest/gtest.h>
#include <vector>

namespace hamdb
{

    class HeapIteratorTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            db_path_ = "test_iterator.db";
            if (std::filesystem::exists(db_path_))
            {
                std::filesystem::remove(db_path_);
            }
            dm_ = std::make_unique<DiskManager>(db_path_);
            ASSERT_EQ(dm_->createDatabase(), Status::Ok);
            ASSERT_EQ(dm_->openDatabase(), Status::Ok);
        }

        void TearDown() override
        {
            (void)dm_->closeDatabase();
            dm_.reset();
            if (std::filesystem::exists(db_path_))
            {
                std::filesystem::remove(db_path_);
            }
        }

        Tuple createTuple(std::string_view sv)
        {
            std::span<const std::byte> span{reinterpret_cast<const std::byte*>(sv.data()),
                                            sv.size()}; // NOLINT
            return Tuple(span);
        }

        std::string db_path_;
        std::unique_ptr<DiskManager> dm_;
    };

    TEST_F(HeapIteratorTest, EmptyHeapHasEqualBeginAndEnd)
    {
        auto heap_opt = TableHeap::create(*dm_);
        ASSERT_TRUE(heap_opt.has_value());
        auto& heap = heap_opt.value();

        auto it = heap.begin();
        auto end = heap.end();

        EXPECT_TRUE(it == end);
    }

    TEST_F(HeapIteratorTest, IteratorYieldsInsertedTuples)
    {
        auto heap_opt = TableHeap::create(*dm_);
        ASSERT_TRUE(heap_opt.has_value());
        auto& heap = heap_opt.value();

        std::vector<std::string> data = {"apple", "banana", "cherry"};
        for (const auto& s : data)
        {
            RID rid;
            ASSERT_EQ(heap.insertTuple(createTuple(s), rid), Status::Ok);
        }

        std::vector<std::string> output;
        for (auto it = heap.begin(); it != heap.end(); ++it)
        {
            Tuple t = *it;
            auto s = t.data();
            std::string_view sv{reinterpret_cast<const char*>(s.data()), s.size()}; // NOLINT
            output.emplace_back(sv);
        }

        EXPECT_EQ(output, data);
    }

    TEST_F(HeapIteratorTest, IteratorSkipsDeletedTuples)
    {
        auto heap_opt = TableHeap::create(*dm_);
        ASSERT_TRUE(heap_opt.has_value());
        auto& heap = heap_opt.value();

        RID rid1, rid2, rid3;
        ASSERT_EQ(heap.insertTuple(createTuple("first"), rid1), Status::Ok);
        ASSERT_EQ(heap.insertTuple(createTuple("deleted"), rid2), Status::Ok);
        ASSERT_EQ(heap.insertTuple(createTuple("third"), rid3), Status::Ok);

        ASSERT_EQ(heap.deleteTuple(rid2), Status::Ok);

        std::vector<std::string> output;
        for (auto it = heap.begin(); it != heap.end(); ++it)
        {
            Tuple t = *it;
            auto s = t.data();
            std::string_view sv{reinterpret_cast<const char*>(s.data()), s.size()}; // NOLINT
            output.emplace_back(sv);
        }

        std::vector<std::string> expected = {"first", "third"};
        EXPECT_EQ(output, expected);
    }

    TEST_F(HeapIteratorTest, IteratorSpansMultiplePages)
    {
        auto heap_opt = TableHeap::create(*dm_);
        ASSERT_TRUE(heap_opt.has_value());
        auto& heap = heap_opt.value();

        std::string large_str(1500, 'Z');
        Tuple large_tuple = createTuple(large_str);

        // Insert enough to span at least 3 pages
        int count = 5;
        for (int i = 0; i < count; ++i)
        {
            RID rid;
            ASSERT_EQ(heap.insertTuple(large_tuple, rid), Status::Ok);
        }

        EXPECT_GT(heap.getPageCount(), 1u);

        int observed = 0;
        for (auto it = heap.begin(); it != heap.end(); ++it)
        {
            observed++;
            EXPECT_EQ((*it).size(), 1500u);
        }

        EXPECT_EQ(observed, count);
    }

} // namespace hamdb
