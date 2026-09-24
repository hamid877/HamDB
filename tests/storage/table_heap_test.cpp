#include "storage/table_heap.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <vector>

namespace hamdb
{

    class TableHeapTest : public ::testing::Test
    {
    protected:
        void SetUp() override {
            auto const* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
            std::string test_name = test_info ? test_info->name() : "unknown";
            auto unique = std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count());

            db_path_ = std::filesystem::temp_directory_path() /
                    ("hamdb-tableheap-" + test_name + "-" + unique + ".hamdb");

            dm_ = std::make_unique<DiskManager>(db_path_);
            ASSERT_EQ(dm_->createDatabase(), Status::Ok);
            ASSERT_EQ(dm_->openDatabase(), Status::Ok);
        }

        void TearDown() override {
            dm_.reset();
            std::error_code ec;
            std::filesystem::remove(db_path_, ec);
        }

        Tuple createTuple(std::string_view sv)
        {
            std::span<const std::byte> span{reinterpret_cast<const std::byte*>(sv.data()), sv.size()}; // NOLINT
            return Tuple(span);
        }

        std::string db_path_;
        std::unique_ptr<DiskManager> dm_;
    };

    TEST_F(TableHeapTest, CreateInitializesHeap)
    {
        auto heap_opt = TableHeap::create(*dm_);
        ASSERT_TRUE(heap_opt.has_value());

        auto& heap = heap_opt.value();
        EXPECT_NE(heap.getFirstPageId(), kInvalidPageId);
        EXPECT_EQ(heap.getPageCount(), 1u);
        EXPECT_EQ(heap.getTupleCount(), 0u);
    }

    TEST_F(TableHeapTest, InsertTupleReadsBackCorrectly)
    {
        auto heap_opt = TableHeap::create(*dm_);
        ASSERT_TRUE(heap_opt.has_value());
        auto& heap = heap_opt.value();

        RID rid;
        ASSERT_EQ(heap.insertTuple(createTuple("hello_table_heap"), rid), Status::Ok);

        EXPECT_EQ(heap.getTupleCount(), 1u);
        EXPECT_EQ(heap.getPageCount(), 1u);

        Tuple out;
        ASSERT_EQ(heap.readTuple(rid, out), Status::Ok);

        auto s = out.data();
        std::string_view sv{reinterpret_cast<const char*>(s.data()), s.size()}; // NOLINT
        EXPECT_EQ(sv, "hello_table_heap");
    }

    TEST_F(TableHeapTest, MultipleInsertsAllocatesNewPages)
    {
        auto heap_opt = TableHeap::create(*dm_);
        ASSERT_TRUE(heap_opt.has_value());
        auto& heap = heap_opt.value();

        // 4096 page size - headers. Inserting 1000 byte tuples will force multiple pages.
        std::string large_str(1000, 'X');
        Tuple large_tuple = createTuple(large_str);

        std::vector<RID> rids;
        for (int i = 0; i < 10; ++i)
        {
            RID rid;
            ASSERT_EQ(heap.insertTuple(large_tuple, rid), Status::Ok);
            rids.push_back(rid);
        }

        EXPECT_GT(heap.getPageCount(), 1u);
        EXPECT_EQ(heap.getTupleCount(), 10u);

        // Verify all can be read
        for (const auto& rid : rids)
        {
            Tuple out;
            ASSERT_EQ(heap.readTuple(rid, out), Status::Ok);
            EXPECT_EQ(out.size(), 1000u);
        }
    }

    TEST_F(TableHeapTest, DeleteTupleRemovesDataAndUpdatesCount)
    {
        auto heap_opt = TableHeap::create(*dm_);
        ASSERT_TRUE(heap_opt.has_value());
        auto& heap = heap_opt.value();

        RID rid;
        ASSERT_EQ(heap.insertTuple(createTuple("to_delete"), rid), Status::Ok);
        EXPECT_EQ(heap.getTupleCount(), 1u);

        ASSERT_EQ(heap.deleteTuple(rid), Status::Ok);
        EXPECT_EQ(heap.getTupleCount(), 0u);

        Tuple out;
        EXPECT_EQ(heap.readTuple(rid, out), Status::InvalidArg); // marked deleted
    }

    TEST_F(TableHeapTest, OpenExistingHeapRecoversMetadata)
    {
        PageId first_page;
        {
            auto heap_opt = TableHeap::create(*dm_);
            ASSERT_TRUE(heap_opt.has_value());
            auto& heap = heap_opt.value();
            first_page = heap.getFirstPageId();

            RID rid1, rid2;
            ASSERT_EQ(heap.insertTuple(createTuple("tuple1"), rid1), Status::Ok);
            ASSERT_EQ(heap.insertTuple(createTuple("tuple2"), rid2), Status::Ok);
        } // heap goes out of scope

        // Now open it
        auto opened_opt = TableHeap::open(*dm_, first_page);
        ASSERT_TRUE(opened_opt.has_value());
        auto& opened = opened_opt.value();

        EXPECT_EQ(opened.getFirstPageId(), first_page);
        EXPECT_EQ(opened.getPageCount(), 1u);
        EXPECT_EQ(opened.getTupleCount(), 2u);
    }

} // namespace hamdb
