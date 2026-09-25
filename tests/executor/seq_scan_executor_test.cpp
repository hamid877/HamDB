#include "buffer/buffer_pool_manager.hpp"
#include "catalog/catalog_manager.hpp"
#include "executor/executor_context.hpp"
#include "executor/seq_scan_executor.hpp"
#include "storage/disk_manager.hpp"
#include "transaction/lock_manager.hpp"
#include "transaction/mvcc_manager.hpp"
#include "transaction/transaction_manager.hpp"
#include "wal/log_manager.hpp"

#include <filesystem>
#include <gtest/gtest.h>
#include <vector>

namespace hamdb
{
    namespace
    {

        std::vector<std::byte> makePayload(std::string_view s)
        {
            std::vector<std::byte> out;
            out.reserve(s.size());
            for (char c : s)
            {
                out.push_back(static_cast<std::byte>(c));
            }
            return out;
        }

        bool payloadEq(const Tuple& t, std::string_view b)
        {
            if (t.size() != b.size())
                return false;
            for (std::size_t i = 0; i < t.size(); ++i)
            {
                if (static_cast<char>(t.data()[i]) != b[i])
                    return false;
            }
            return true;
        }

        class SeqScanExecutorTest : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                const std::string db_path = "test_seq_scan.hamdb";
                if (std::filesystem::exists(db_path))
                {
                    std::filesystem::remove(db_path);
                }

                // Setup initial database page manually since TableHeap::open relies on it.
                disk_manager_ = std::make_unique<DiskManager>(db_path);
                auto st = disk_manager_->createDatabase();
                ASSERT_EQ(st, Status::Ok);
                st = disk_manager_->openDatabase();
                ASSERT_EQ(st, Status::Ok);

                bpm_ = std::make_unique<BufferPoolManager>(10, *disk_manager_);
                catalog_ = std::make_unique<CatalogManager>(bpm_.get());

                // Create table
                std::vector<Column> cols;
                cols.emplace_back("col1", ColumnType::Varchar);
                Schema schema(std::move(cols));

                Status s = catalog_->createTable("test_table", schema, table_info_);
                ASSERT_EQ(s, Status::Ok);

                // Explicitly initialize the TableHeap on the allocated page.
                auto heap_opt = TableHeap::create(*disk_manager_);
                ASSERT_TRUE(heap_opt.has_value());

                // We override the root page in the catalog for testing purposes
                // because CatalogManager just allocates a raw page and doesn't format it as a
                // TableHeap. Wait, TableHeap::create allocates its own page and it might be page 2
                // or 3. We just need to make sure the root page is correct for SeqScanExecutor. But
                // TableInfo properties are const. We can just create TableHeap first!
            }

            void TearDown() override
            {
                catalog_.reset();
                bpm_.reset();
                disk_manager_.reset();
                std::filesystem::remove("test_seq_scan.hamdb");
            }

            std::unique_ptr<DiskManager> disk_manager_;
            std::unique_ptr<BufferPoolManager> bpm_;
            std::unique_ptr<CatalogManager> catalog_;
            TableInfo* table_info_{nullptr};
            TransactionManager txn_mgr_;
            MvccManager mvcc_;
            LockManager lock_mgr_;
            LogManager log_mgr_;
        };

        TEST_F(SeqScanExecutorTest, EmptyTable)
        {
            auto* txn = txn_mgr_.begin();
            ExecutorContext exec_ctx(txn, catalog_.get(), bpm_.get(), &mvcc_, disk_manager_.get(),
                                     &lock_mgr_, &log_mgr_);

            // We override root page by creating a fresh table heap.
            auto heap = TableHeap::create(*disk_manager_);
            ASSERT_TRUE(heap.has_value());
            TableInfo info(table_info_->getTableId(), table_info_->getTableName(),
                           heap->getFirstPageId(), table_info_->getIndexRootPage(),
                           table_info_->getSchema());

            SeqScanExecutor executor(&exec_ctx, &info);
            executor.init();

            Tuple tuple;
            RID rid;
            EXPECT_FALSE(executor.next(&tuple, &rid));

            txn_mgr_.commit(txn);
        }

        TEST_F(SeqScanExecutorTest, ScanBaseTuplesOnly)
        {
            auto heap = TableHeap::create(*disk_manager_);
            ASSERT_TRUE(heap.has_value());
            TableInfo info(table_info_->getTableId(), table_info_->getTableName(),
                           heap->getFirstPageId(), table_info_->getIndexRootPage(),
                           table_info_->getSchema());

            RID rid1, rid2;
            ASSERT_EQ(heap->insertTuple(Tuple(makePayload("row1")), rid1), Status::Ok);
            ASSERT_EQ(heap->insertTuple(Tuple(makePayload("row2")), rid2), Status::Ok);

            auto* txn = txn_mgr_.begin();
            ExecutorContext exec_ctx(txn, catalog_.get(), bpm_.get(), &mvcc_, disk_manager_.get(),
                                     &lock_mgr_, &log_mgr_);
            SeqScanExecutor executor(&exec_ctx, &info);

            executor.init();
            Tuple tuple;
            RID rid;

            ASSERT_TRUE(executor.next(&tuple, &rid));
            EXPECT_EQ(rid, rid1);
            EXPECT_TRUE(payloadEq(tuple, "row1"));

            ASSERT_TRUE(executor.next(&tuple, &rid));
            EXPECT_EQ(rid, rid2);
            EXPECT_TRUE(payloadEq(tuple, "row2"));

            EXPECT_FALSE(executor.next(&tuple, &rid));

            txn_mgr_.commit(txn);
        }

        TEST_F(SeqScanExecutorTest, ScanMvccVersions)
        {
            auto heap = TableHeap::create(*disk_manager_);
            ASSERT_TRUE(heap.has_value());
            TableInfo info(table_info_->getTableId(), table_info_->getTableName(),
                           heap->getFirstPageId(), table_info_->getIndexRootPage(),
                           table_info_->getSchema());

            RID rid1, rid2;
            ASSERT_EQ(heap->insertTuple(Tuple(makePayload("row1_old")), rid1), Status::Ok);
            ASSERT_EQ(heap->insertTuple(Tuple(makePayload("row2_old")), rid2), Status::Ok);

            // Update via MVCC
            auto* update_txn = txn_mgr_.begin();
            ASSERT_TRUE(mvcc_.insert(update_txn, rid1, makePayload("row1_new")));
            ASSERT_TRUE(mvcc_.insert(update_txn, rid2, makePayload("row2_new")));
            ASSERT_TRUE(mvcc_.remove(update_txn, rid2));
            mvcc_.commit(update_txn);
            txn_mgr_.commit(update_txn);

            auto* scan_txn = txn_mgr_.begin();
            ExecutorContext exec_ctx(scan_txn, catalog_.get(), bpm_.get(), &mvcc_,
                                     disk_manager_.get(), &lock_mgr_, &log_mgr_);
            SeqScanExecutor executor(&exec_ctx, &info);

            executor.init();
            Tuple tuple;
            RID rid;

            ASSERT_TRUE(executor.next(&tuple, &rid));
            EXPECT_EQ(rid, rid1);
            EXPECT_TRUE(payloadEq(tuple, "row1_new"));

            // rid2 is deleted so we shouldn't see it
            EXPECT_FALSE(executor.next(&tuple, &rid));

            txn_mgr_.commit(scan_txn);
        }

    } // namespace
} // namespace hamdb
