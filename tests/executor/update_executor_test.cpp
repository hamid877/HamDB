#include "executor/update_executor.hpp"
#include "executor/constant_expression.hpp"
#include "executor/executor_context.hpp"
#include "catalog/catalog_manager.hpp"
#include "storage/disk_manager.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include "transaction/transaction_manager.hpp"
#include "transaction/mvcc_manager.hpp"
#include "transaction/lock_manager.hpp"
#include "wal/log_manager.hpp"
#include "utils/serializer.hpp"
#include "utils/deserializer.hpp"
#include "storage/table_heap.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <vector>

namespace hamdb {
namespace {

class MockExecutor : public AbstractExecutor {
public:
    MockExecutor(std::vector<std::pair<Tuple, RID>> tuples, Schema schema) 
        : tuples_(std::move(tuples)), schema_(std::move(schema)) {}
        
    void init() override { 
        iter_ = tuples_.begin(); 
    }
    
    bool next(Tuple* tuple, RID* rid) override {
        if (iter_ == tuples_.end()) return false;
        *tuple = iter_->first;
        *rid = iter_->second;
        ++iter_;
        return true;
    }
    
    const Schema& outputSchema() const override { return schema_; }

private:
    std::vector<std::pair<Tuple, RID>> tuples_;
    Schema schema_;
    std::vector<std::pair<Tuple, RID>>::iterator iter_;
};

struct BPlusTreeLayout {
    BufferPoolManager& bpm_;
    PageId root_page_id_;
};

class UpdateExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        const std::string db_path = "test_update.hamdb";
        if (std::filesystem::exists(db_path)) {
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
        cols.emplace_back("id", ColumnType::Integer);
        cols.emplace_back("val", ColumnType::Varchar);
        schema_ = std::make_unique<Schema>(std::move(cols));
        
        Status s = catalog_->createTable("test_table", *schema_, table_info_);
        ASSERT_EQ(s, Status::Ok);
    }

    void TearDown() override {
        catalog_.reset();
        bpm_.reset();
        disk_manager_.reset();
        std::filesystem::remove("test_update.hamdb");
    }

    Tuple makeTuple(int32_t id, const std::string& val) {
        std::vector<std::byte> buf(64);
        Serializer ser(buf);
        (void)ser.writeInt32(id);
        (void)ser.writeString(val);
        return Tuple(std::span<const std::byte>(buf.data(), ser.position()));
    }

    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> bpm_;
    std::unique_ptr<CatalogManager> catalog_;
    std::unique_ptr<Schema> schema_;
    TableInfo* table_info_{nullptr};
    TransactionManager txn_mgr_;
    MvccManager mvcc_;
    LockManager lock_mgr_;
    LogManager log_mgr_;
};

TEST_F(UpdateExecutorTest, UpdateSingleTuple) {
    auto* txn = txn_mgr_.begin();
    ExecutorContext exec_ctx(txn, catalog_.get(), bpm_.get(), &mvcc_, disk_manager_.get(), &lock_mgr_, &log_mgr_);
    
    auto heap = TableHeap::create(*disk_manager_);
    ASSERT_TRUE(heap.has_value());
    
    BPlusTree tree(*bpm_);
    tree.create(); 
    
    RID rid1(1, 1);
    RID rid2(1, 2);
    Tuple t1 = makeTuple(10, "row1");
    Tuple t2 = makeTuple(20, "row2");
    
    std::vector<std::byte> d1(t1.data().begin(), t1.data().end());
    std::vector<std::byte> d2(t2.data().begin(), t2.data().end());
    ASSERT_TRUE(mvcc_.insert(txn, rid1, std::move(d1)));
    ASSERT_TRUE(mvcc_.insert(txn, rid2, std::move(d2)));
    (void)tree.insert(10, rid1);
    (void)tree.insert(20, rid2);

    PageId tree_root = reinterpret_cast<BPlusTreeLayout*>(&tree)->root_page_id_;
    TableInfo info(table_info_->getTableId(), table_info_->getTableName(), heap->getFirstPageId(), tree_root, *schema_);

    std::vector<std::pair<Tuple, RID>> tuples;
    tuples.push_back({t1, rid1});
    // only feed t1 to child_exec to update it
    
    auto child_exec = std::make_unique<MockExecutor>(tuples, *schema_);
    
    std::vector<std::unique_ptr<Expression>> target_exprs;
    target_exprs.push_back(std::make_unique<ConstantExpression>(Value(999))); // new ID
    target_exprs.push_back(std::make_unique<ConstantExpression>(Value("updated"))); // new val
    
    UpdateExecutor executor(&exec_ctx, &info, std::move(child_exec), std::move(target_exprs));
    
    executor.init();
    
    Tuple result_tuple;
    RID rid;
    ASSERT_TRUE(executor.next(&result_tuple, &rid));
    
    Deserializer des(result_tuple.data());
    int32_t count = 0;
    ASSERT_EQ(des.readInt32(count), Status::Ok);
    EXPECT_EQ(count, 1);
    
    EXPECT_FALSE(executor.next(&result_tuple, &rid));
    
    // Verify tree
    EXPECT_FALSE(tree.getValue(10).has_value());
    EXPECT_TRUE(tree.getValue(20).has_value());
    EXPECT_TRUE(tree.getValue(999).has_value());
    
    txn_mgr_.commit(txn);
}

} // namespace
} // namespace hamdb
