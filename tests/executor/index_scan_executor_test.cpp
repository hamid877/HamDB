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
#include <cstring>

#include "executor/comparison_expression.hpp"
#include "executor/column_value_expression.hpp"
#include "executor/constant_expression.hpp"

namespace hamdb
{
    namespace
    {

        std::vector<std::byte> makePayload(int32_t val)
        {
            std::vector<std::byte> out(4);
            std::memcpy(out.data(), &val, 4);
            return out;
        }

        std::unique_ptr<Expression> makeEqPredicate(int32_t val) {
            return std::make_unique<ComparisonExpression>(
                ComparisonType::Equal,
                std::make_unique<ColumnValueExpression>(0),
                std::make_unique<ConstantExpression>(Value(val))
            );
        }

        bool payloadEq(const Tuple& t, int32_t val)
        {
            if (t.size() != 4)
                return false;
            int32_t act = 0;
            std::memcpy(&act, t.data().data(), 4);
            return act == val;
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
                cols.emplace_back("col1", ColumnType::Integer);
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

            IndexScanExecutor executor(&exec_ctx, &info, makeEqPredicate(42));
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
            ASSERT_EQ(heap->insertTuple(Tuple(makePayload(10)), rid1), Status::Ok);
            ASSERT_EQ(heap->insertTuple(Tuple(makePayload(20)), rid2), Status::Ok);

            ASSERT_EQ(tree.insert(10, rid1), Status::Ok);
            ASSERT_EQ(tree.insert(20, rid2), Status::Ok);

            PageId tree_root = reinterpret_cast<BPlusTreeLayout*>(&tree)->root_page_id_;
            TableInfo info(table_info_->getTableId(), table_info_->getTableName(),
                           heap->getFirstPageId(), tree_root, table_info_->getSchema());

            auto* txn = txn_mgr_.begin();
            ExecutorContext exec_ctx(txn, catalog_.get(), bpm_.get(), &mvcc_, disk_manager_.get(),
                                     &lock_mgr_, &log_mgr_);

            // Search for existing key
            IndexScanExecutor executor1(&exec_ctx, &info, makeEqPredicate(20));
            executor1.init();
            Tuple tuple;
            RID rid;
            ASSERT_TRUE(executor1.next(&tuple, &rid));
            EXPECT_EQ(rid, rid2);
            EXPECT_TRUE(payloadEq(tuple, 20));
            EXPECT_FALSE(executor1.next(&tuple, &rid));

            // Search for non-existent key
            IndexScanExecutor executor2(&exec_ctx, &info, makeEqPredicate(999));
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
            ASSERT_EQ(heap->insertTuple(Tuple(makePayload(10)), rid1), Status::Ok);
            ASSERT_EQ(tree.insert(10, rid1), Status::Ok);

            PageId tree_root = reinterpret_cast<BPlusTreeLayout*>(&tree)->root_page_id_;
            TableInfo info(table_info_->getTableId(), table_info_->getTableName(),
                           heap->getFirstPageId(), tree_root, table_info_->getSchema());

            // Update via MVCC
            auto* update_txn = txn_mgr_.begin();
            ASSERT_TRUE(mvcc_.insert(update_txn, rid1, makePayload(10)));
            mvcc_.commit(update_txn);
            txn_mgr_.commit(update_txn);

            auto* scan_txn = txn_mgr_.begin();
            ExecutorContext exec_ctx(scan_txn, catalog_.get(), bpm_.get(), &mvcc_,
                                     disk_manager_.get(), &lock_mgr_, &log_mgr_);
            IndexScanExecutor executor(&exec_ctx, &info, makeEqPredicate(10));

            executor.init();
            Tuple tuple;
            RID rid;

            ASSERT_TRUE(executor.next(&tuple, &rid));
            EXPECT_EQ(rid, rid1);
            EXPECT_TRUE(payloadEq(tuple, 10));
            EXPECT_FALSE(executor.next(&tuple, &rid));

            // Delete via MVCC
            auto* del_txn = txn_mgr_.begin();
            ASSERT_TRUE(mvcc_.remove(del_txn, rid1));
            mvcc_.commit(del_txn);
            txn_mgr_.commit(del_txn);

            auto* scan_txn2 = txn_mgr_.begin();
            ExecutorContext exec_ctx2(scan_txn2, catalog_.get(), bpm_.get(), &mvcc_,
                                      disk_manager_.get(), &lock_mgr_, &log_mgr_);
            IndexScanExecutor executor2(&exec_ctx2, &info, makeEqPredicate(10));

            executor2.init();
            // It should not find it as it is deleted in MVCC
            EXPECT_FALSE(executor2.next(&tuple, &rid));

            txn_mgr_.commit(scan_txn);
            txn_mgr_.commit(scan_txn2);
        }

    } // namespace
} // namespace hamdb
