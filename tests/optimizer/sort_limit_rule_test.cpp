#include "optimizer/rule_executor.hpp"
#include "optimizer/sort_limit_rule.hpp"
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

class SortLimitRuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_ = "test_sort_limit_rule.hamdb";
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
        rule_executor_->addRule(std::make_unique<SortLimitRule>());

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

TEST_F(SortLimitRuleTest, SortElimination) {
    TableInfo* table_info;
    (void)catalog_->getTable("test_table", table_info);
    
    auto index_scan = std::make_unique<planner::LogicalIndexScanNode>(table_info->getSchema(), "test_table", "test_table", nullptr);
    
    std::vector<std::pair<std::unique_ptr<hamdb::Expression>, bool>> order_by;
    order_by.emplace_back(std::make_unique<hamdb::ColumnValueExpression>(0), true); // ASC on col 0
    
    auto sort = std::make_unique<planner::SortPlanNode>(table_info->getSchema(), std::move(order_by));
    sort->addChild(std::move(index_scan));
    
    std::unique_ptr<planner::LogicalPlanNode> root = std::move(sort);
    
    auto optimized_root = rule_executor_->optimize(std::move(root));
    
    // Sort should be eliminated, root should be LogicalIndexScanNode
    ASSERT_EQ(optimized_root->getType(), planner::LogicalPlanType::INDEX_SCAN);
}

TEST_F(SortLimitRuleTest, LimitPushdownSeqScan) {
    TableInfo* table_info;
    (void)catalog_->getTable("test_table", table_info);
    
    auto seq_scan = std::make_unique<planner::SeqScanPlanNode>(table_info->getSchema(), "test_table", "test_table", nullptr);
    
    auto limit = std::make_unique<planner::LimitPlanNode>(table_info->getSchema(), std::make_unique<hamdb::ConstantExpression>(Value(2)), std::make_unique<hamdb::ConstantExpression>(Value(1)));
    limit->addChild(std::move(seq_scan));
    
    std::unique_ptr<planner::LogicalPlanNode> root = std::move(limit);
    
    auto optimized_root = rule_executor_->optimize(std::move(root));
    
    // Limit should be eliminated, root should be SeqScanPlanNode
    ASSERT_EQ(optimized_root->getType(), planner::LogicalPlanType::SEQ_SCAN);
    auto* opt_scan = static_cast<planner::SeqScanPlanNode*>(optimized_root.get());
    
    ASSERT_NE(opt_scan->getLimit(), nullptr);
    ASSERT_EQ(opt_scan->getLimit()->evaluate(Tuple{}, Schema(std::vector<Column>{})).getAsInteger(), 2);
    ASSERT_NE(opt_scan->getOffset(), nullptr);
    ASSERT_EQ(opt_scan->getOffset()->evaluate(Tuple{}, Schema(std::vector<Column>{})).getAsInteger(), 1);
}

TEST_F(SortLimitRuleTest, LimitPushdownExecution) {
    TableInfo* table_info;
    (void)catalog_->getTable("test_table", table_info);
    
    auto txn = txn_manager_->begin();
    ExecutorContext exec_ctx(txn, catalog_.get(), bpm_.get(), mvcc_manager_.get(), disk_manager_.get(), lock_manager_.get(), log_manager_.get());
    
    auto insert_tuple = [&](int id, int val) {
        std::vector<std::byte> buf(8);
        Serializer ser(buf);
        (void)ser.writeInt32(id);
        (void)ser.writeInt32(val);
        Tuple t(std::span<const std::byte>(buf.data(), ser.position()));
        return t;
    };
    
    auto heap_opt = TableHeap::create(*disk_manager_);
    auto& heap = *heap_opt;
    *table_info = TableInfo(table_info->getTableId(), table_info->getTableName(), heap.getFirstPageId(), kInvalidPageId, table_info->getSchema());

    RID r;
    (void)heap.insertTuple(insert_tuple(1, 10), r);
    (void)heap.insertTuple(insert_tuple(2, 20), r);
    (void)heap.insertTuple(insert_tuple(3, 30), r);
    (void)heap.insertTuple(insert_tuple(4, 40), r);
    (void)heap.insertTuple(insert_tuple(5, 50), r);
    
    txn_manager_->commit(txn);
    
    auto seq_scan = std::make_unique<planner::SeqScanPlanNode>(table_info->getSchema(), "test_table", "test_table", nullptr);
    auto limit = std::make_unique<planner::LimitPlanNode>(table_info->getSchema(), std::make_unique<hamdb::ConstantExpression>(Value(2)), std::make_unique<hamdb::ConstantExpression>(Value(1)));
    limit->addChild(std::move(seq_scan));
    
    std::unique_ptr<planner::LogicalPlanNode> root = std::move(limit);
    auto opt_root = rule_executor_->optimize(std::move(root));
    auto phys_root = physical_planner_->plan(std::move(opt_root));
    auto exec = planner::ExecutorFactory::createExecutor(&exec_ctx, std::move(phys_root));
    
    exec->init();
    Tuple t;
    RID tr;
    int count = 0;
    while(exec->next(&t, &tr)) {
        count++;
    }
    
    EXPECT_EQ(count, 2);
}

} // namespace hamdb::optimizer
