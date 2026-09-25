#include "buffer/buffer_pool_manager.hpp"
#include "catalog/catalog_manager.hpp"
#include "executor/executor_context.hpp"
#include "executor/index_scan_executor.hpp"
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

        struct BPlusTreeLayout
        {
            BufferPoolManager& bpm_;
            PageId root_page_id_;
        };

        class IndexScanExecutorTest : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                const std::string db_path = "test_index_scan.hamdb";
                if (std::filesystem::exists(db_path))
                {
                    std::filesystem::remove(db_path);
                }

                disk_manager_ = std::make_unique<DiskManager>(db_path);
                auto st = disk_manager_->createDatabase();
                ASSERT_EQ(st, Status::Ok);
                st = disk_manager_->openDatabase();
                ASSERT_EQ(st, Status::Ok);

                bpm_ = std::make_unique<BufferPoolManager>(10, *disk_manager_);
                catalog_ = std::make_unique<CatalogManager>(bpm_.get());

                std::vector<Column> cols;
                cols.emplace_back("col1", ColumnType::Varchar);
                Schema schema(std::move(cols));

                Status s = catalog_->createTable("test_table", schema, table_info_);
                ASSERT_EQ(s, Status::Ok);
            }

            void TearDown() override
            {
                catalog_.reset();
                bpm_.reset();
                disk_manager_.reset();
                std::filesystem::remove("test_index_scan.hamdb");
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

        TEST_F(IndexScanExecutorTest, EmptyTable)
        {
            auto* txn = txn_mgr_.begin();
            ExecutorContext exec_ctx(txn, catalog_.get(), bpm_.get(), &mvcc_, disk_manager_.get(),
                                     &lock_mgr_, &log_mgr_);

            auto heap = TableHeap::create(*disk_manager_);
            ASSERT_TRUE(heap.has_value());
            BPlusTree tree(*bpm_);
            tree.create(); // Create root page

            PageId tree_root = reinterpret_cast<BPlusTreeLayout*>(&tree)->root_page_id_;
            TableInfo info(table_info_->getTableId(), table_info_->getTableName(),
                           heap->getFirstPageId(), tree_root, table_info_->getSchema());

            IndexScanExecutor executor(&exec_ctx, &info, 42);
            executor.init();

            Tuple tuple;
            RID rid;
            EXPECT_FALSE(executor.next(&tuple, &rid));

            txn_mgr_.commit(txn);
        }

        TEST_F(IndexScanExecutorTest, ScanBaseTupleOnly)
        {
            auto heap = TableHeap::create(*disk_manager_);
            ASSERT_TRUE(heap.has_value());
            BPlusTree tree(*bpm_);
            tree.create();

            RID rid1, rid2;
            ASSERT_EQ(heap->insertTuple(Tuple(makePayload("row1")), rid1), Status::Ok);
            ASSERT_EQ(heap->insertTuple(Tuple(makePayload("row2")), rid2), Status::Ok);

            ASSERT_EQ(tree.insert(10, rid1), Status::Ok);
            ASSERT_EQ(tree.insert(20, rid2), Status::Ok);

            PageId tree_root = reinterpret_cast<BPlusTreeLayout*>(&tree)->root_page_id_;
            TableInfo info(table_info_->getTableId(), table_info_->getTableName(),
                           heap->getFirstPageId(), tree_root, table_info_->getSchema());

            auto* txn = txn_mgr_.begin();
            ExecutorContext exec_ctx(txn, catalog_.get(), bpm_.get(), &mvcc_, disk_manager_.get(),
                                     &lock_mgr_, &log_mgr_);

            // Search for existing key
            IndexScanExecutor executor1(&exec_ctx, &info, 20);
            executor1.init();
            Tuple tuple;
            RID rid;
            ASSERT_TRUE(executor1.next(&tuple, &rid));
            EXPECT_EQ(rid, rid2);
            EXPECT_TRUE(payloadEq(tuple, "row2"));
            EXPECT_FALSE(executor1.next(&tuple, &rid));

            // Search for non-existent key
            IndexScanExecutor executor2(&exec_ctx, &info, 999);
            executor2.init();
            EXPECT_FALSE(executor2.next(&tuple, &rid));

            txn_mgr_.commit(txn);
        }

        TEST_F(IndexScanExecutorTest, ScanMvccVersions)
        {
            auto heap = TableHeap::create(*disk_manager_);
            ASSERT_TRUE(heap.has_value());
            BPlusTree tree(*bpm_);
            tree.create();

            RID rid1;
            ASSERT_EQ(heap->insertTuple(Tuple(makePayload("row1_old")), rid1), Status::Ok);
            ASSERT_EQ(tree.insert(10, rid1), Status::Ok);

            PageId tree_root = reinterpret_cast<BPlusTreeLayout*>(&tree)->root_page_id_;
            TableInfo info(table_info_->getTableId(), table_info_->getTableName(),
                           heap->getFirstPageId(), tree_root, table_info_->getSchema());

            // Update via MVCC
            auto* update_txn = txn_mgr_.begin();
            ASSERT_TRUE(mvcc_.insert(update_txn, rid1, makePayload("row1_new")));
            mvcc_.commit(update_txn);
            txn_mgr_.commit(update_txn);

            auto* scan_txn = txn_mgr_.begin();
            ExecutorContext exec_ctx(scan_txn, catalog_.get(), bpm_.get(), &mvcc_,
                                     disk_manager_.get(), &lock_mgr_, &log_mgr_);
            IndexScanExecutor executor(&exec_ctx, &info, 10);

            executor.init();
            Tuple tuple;
            RID rid;

            ASSERT_TRUE(executor.next(&tuple, &rid));
            EXPECT_EQ(rid, rid1);
            EXPECT_TRUE(payloadEq(tuple, "row1_new"));
            EXPECT_FALSE(executor.next(&tuple, &rid));

            // Delete via MVCC
            auto* del_txn = txn_mgr_.begin();
            ASSERT_TRUE(mvcc_.remove(del_txn, rid1));
            mvcc_.commit(del_txn);
            txn_mgr_.commit(del_txn);

            auto* scan_txn2 = txn_mgr_.begin();
            ExecutorContext exec_ctx2(scan_txn2, catalog_.get(), bpm_.get(), &mvcc_,
                                      disk_manager_.get(), &lock_mgr_, &log_mgr_);
            IndexScanExecutor executor2(&exec_ctx2, &info, 10);

            executor2.init();
            // It should not find it as it is deleted in MVCC
            EXPECT_FALSE(executor2.next(&tuple, &rid));

            txn_mgr_.commit(scan_txn);
            txn_mgr_.commit(scan_txn2);
        }

    } // namespace
} // namespace hamdb
