#include "optimizer/rule_executor.hpp"
#include "optimizer/index_scan_rule.hpp"
#include "planner/logical_plan.hpp"
#include "planner/logical_index_scan.hpp"
#include "planner/planner.hpp"
#include "planner/physical_planner.hpp"
#include "planner/executor_factory.hpp"
#include "executor/comparison_expression.hpp"
#include "executor/column_value_expression.hpp"
#include "executor/constant_expression.hpp"
#include "catalog/catalog_manager.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include "storage/disk_manager.hpp"
#include "transaction/transaction_manager.hpp"
#include "transaction/lock_manager.hpp"
#include "transaction/mvcc_manager.hpp"
#include "wal/log_manager.hpp"
#include "executor/executor_context.hpp"
#include "storage/table_heap.hpp"
#include "storage/slotted_page.hpp"
#include "index/bplus_tree.hpp"
#include <filesystem>
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include "utils/serializer.hpp"

namespace hamdb::optimizer {

class IndexScanRuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_ = "test_index_scan_optimizer.hamdb";
        if (std::filesystem::exists(test_db_)) {
            std::filesystem::remove(test_db_);
        }
        disk_manager_ = std::make_unique<DiskManager>(test_db_);
        (void)disk_manager_->createDatabase();
        (void)disk_manager_->openDatabase();
        bpm_ = std::make_unique<BufferPoolManager>(10, *disk_manager_);
        catalog_ = std::make_unique<CatalogManager>(bpm_.get());
        
        std::vector<Column> cols = {
            Column("id", ColumnType::Integer),
            Column("val", ColumnType::Integer)
        };
        Schema schema(cols);
        TableInfo* info;
        (void)catalog_->createTable("test_table", schema, info);
        planner_ = std::make_unique<planner::Planner>(catalog_.get());
        physical_planner_ = std::make_unique<planner::PhysicalPlanner>(catalog_.get());
        rule_executor_ = std::make_unique<RuleExecutor>();
        rule_executor_->addRule(std::make_unique<IndexScanRule>(catalog_.get()));

        txn_manager_ = std::make_unique<TransactionManager>();
        lock_manager_ = std::make_unique<LockManager>();
        log_manager_ = std::make_unique<LogManager>();
        mvcc_manager_ = std::make_unique<MvccManager>();
    }
    
    void TearDown() override {
        if (std::filesystem::exists(test_db_)) {
            std::filesystem::remove(test_db_);
        }
    }

    std::string test_db_;
    
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> bpm_;
    std::unique_ptr<CatalogManager> catalog_;
    std::unique_ptr<planner::Planner> planner_;
    std::unique_ptr<planner::PhysicalPlanner> physical_planner_;
    std::unique_ptr<RuleExecutor> rule_executor_;

    std::unique_ptr<TransactionManager> txn_manager_;
    std::unique_ptr<LockManager> lock_manager_;
    std::unique_ptr<LogManager> log_manager_;
    std::unique_ptr<MvccManager> mvcc_manager_;
};

TEST_F(IndexScanRuleTest, RewriteToLogicalIndexScan) {
    TableInfo* table_info;
    (void)catalog_->getTable("test_table", table_info);
    
    // Create logical plan manually
    auto comp_expr = std::make_unique<hamdb::ComparisonExpression>(
        hamdb::ComparisonType::Equal,
        std::make_unique<hamdb::ColumnValueExpression>(0),
        std::make_unique<hamdb::ConstantExpression>(Value(5))
    );
    auto seq_scan = std::make_unique<planner::SeqScanPlanNode>(table_info->getSchema(), "test_table", "test_table", std::move(comp_expr));
    
    std::unique_ptr<planner::LogicalPlanNode> root = std::move(seq_scan);
    
    // Optimize
    auto optimized_root = rule_executor_->optimize(std::move(root));
    
    // Verify rewrite
    ASSERT_EQ(optimized_root->getType(), planner::LogicalPlanType::INDEX_SCAN);
    auto* optimized_idx = dynamic_cast<planner::LogicalIndexScanNode*>(optimized_root.get());
    ASSERT_NE(optimized_idx, nullptr);
    ASSERT_NE(optimized_idx->getPredicate(), nullptr);
}

TEST_F(IndexScanRuleTest, IndexScanExecutionResults) {
    TableInfo* table_info;
    (void)catalog_->getTable("test_table", table_info);
    
    // Insert some tuples
    auto txn = txn_manager_->begin();
    ExecutorContext exec_ctx(txn, catalog_.get(), bpm_.get(), mvcc_manager_.get(), disk_manager_.get(), lock_manager_.get(), log_manager_.get());
    
    // Create dummy payloads manually or via tuple insertion
    auto insert_tuple = [&](int id, int val) {
        std::vector<std::byte> buf(8);
        Serializer ser(buf);
        (void)ser.writeInt32(id);
        (void)ser.writeInt32(val);
        Tuple t(std::span<const std::byte>(buf.data(), ser.position()));
        return t;
    };
    
    struct BPlusTreeLayout
    {
        BufferPoolManager& bpm_;
        PageId root_page_id_;
    };

    auto heap_opt = TableHeap::create(*disk_manager_);
    auto& heap = *heap_opt;
    BPlusTree tree(*bpm_);
    tree.create();
    PageId tree_root = reinterpret_cast<BPlusTreeLayout*>(&tree)->root_page_id_;
    
    *table_info = TableInfo(table_info->getTableId(), table_info->getTableName(),
                            heap.getFirstPageId(), tree_root, table_info->getSchema());

    RID r1, r2, r3, r4;
    (void)heap.insertTuple(insert_tuple(1, 10), r1);
    (void)heap.insertTuple(insert_tuple(5, 50), r2);
    (void)heap.insertTuple(insert_tuple(10, 100), r3);
    (void)heap.insertTuple(insert_tuple(15, 150), r4);
    
    (void)tree.insert(1, r1);
    (void)tree.insert(5, r2);
    (void)tree.insert(10, r3);
    (void)tree.insert(15, r4);

    txn_manager_->commit(txn);
    
    // Test Equal: id = 5
    auto test_scan = [&](ComparisonType type, int val, int expected_count) {
        auto comp_expr = std::make_unique<hamdb::ComparisonExpression>(
            type,
            std::make_unique<hamdb::ColumnValueExpression>(0),
            std::make_unique<hamdb::ConstantExpression>(Value(val))
        );
        auto seq_scan = std::make_unique<planner::SeqScanPlanNode>(table_info->getSchema(), "test_table", "test_table", std::move(comp_expr));
        
        std::unique_ptr<planner::LogicalPlanNode> opt_root = rule_executor_->optimize(std::move(seq_scan));
        auto opt_phys = physical_planner_->plan(std::move(opt_root));
        auto opt_exec = planner::ExecutorFactory::createExecutor(&exec_ctx, std::move(opt_phys));
        
        opt_exec->init();
        Tuple t;
        RID r;
        int count = 0;
        while(opt_exec->next(&t, &r)) {
            count++;
        }
        EXPECT_EQ(count, expected_count);
    };

    test_scan(ComparisonType::Equal, 5, 1);
    test_scan(ComparisonType::GreaterThan, 5, 2);
    test_scan(ComparisonType::GreaterThanOrEqual, 5, 3);
    test_scan(ComparisonType::LessThan, 10, 2);
    test_scan(ComparisonType::LessThanOrEqual, 10, 3);
}

} // namespace hamdb::optimizer
