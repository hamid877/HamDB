#include "optimizer/rule_executor.hpp"
#include "optimizer/predicate_pushdown_rule.hpp"
#include "planner/physical_planner.hpp"
#include "planner/planner.hpp"
#include "planner/executor_factory.hpp"
#include "binder/bound_statement.hpp"
#include "binder/bound_expression.hpp"
#include "catalog/catalog_manager.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include "storage/disk_manager.hpp"
#include "transaction/transaction_manager.hpp"
#include "transaction/lock_manager.hpp"
#include "transaction/mvcc_manager.hpp"
#include "wal/log_manager.hpp"
#include "executor/executor_context.hpp"
#include "executor/abstract_executor.hpp"
#include "storage/table_heap.hpp"
#include "storage/slotted_page.hpp"
#include <filesystem>
#include <cstring>
#include <gtest/gtest.h>
#include <memory>

namespace hamdb::optimizer {

class PredicatePushdownTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_ = "test_optimizer.hamdb";
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
        rule_executor_->addRule(std::make_unique<PredicatePushdownRule>());

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

TEST_F(PredicatePushdownTest, PushdownFilterToSeqScan) {
    TableInfo* table_info;
    (void)catalog_->getTable("test_table", table_info);
    
    // Create logical plan manually
    auto seq_scan = std::make_unique<planner::SeqScanPlanNode>(table_info->getSchema(), "test_table", "test_table");
    
    auto comp_expr = std::make_unique<hamdb::ComparisonExpression>(
        hamdb::ComparisonType::Equal,
        std::make_unique<hamdb::ColumnValueExpression>(0),
        std::make_unique<hamdb::ConstantExpression>(Value(5))
    );
    auto filter = std::make_unique<planner::FilterPlanNode>(table_info->getSchema(), std::move(comp_expr));
    filter->addChild(std::move(seq_scan));
    
    std::unique_ptr<planner::LogicalPlanNode> root = std::move(filter);
    
    // Optimize
    auto optimized_root = rule_executor_->optimize(std::move(root));
    
    // Verify rewrite
    ASSERT_EQ(optimized_root->getType(), planner::LogicalPlanType::SEQ_SCAN);
    auto* optimized_seq = dynamic_cast<planner::SeqScanPlanNode*>(optimized_root.get());
    ASSERT_NE(optimized_seq, nullptr);
    ASSERT_NE(optimized_seq->getPredicate(), nullptr);
}

TEST_F(PredicatePushdownTest, ExecutionResultsIdentical) {
    TableInfo* table_info;
    (void)catalog_->getTable("test_table", table_info);
    
    // Insert some tuples
    auto txn = txn_manager_->begin();
    ExecutorContext exec_ctx(txn, catalog_.get(), bpm_.get(), mvcc_manager_.get(), disk_manager_.get(), lock_manager_.get(), log_manager_.get());
    
    // We will just create dummy payloads
    std::vector<std::byte> payload1 = {std::byte{1}};
    std::vector<std::byte> payload2 = {std::byte{2}};
    
    // Initialize the heap root page
    {
        WritePageGuard guard;
        (void)bpm_->fetchPageWrite(table_info->getHeapRootPage(), guard);
        Page page(PageHeader(table_info->getHeapRootPage(), PageType::Table));
        SlottedPage sp(page);
        (void)sp.initialize();
        std::memcpy(guard.pageMut().data().data(), page.data().data(), Page::kSize);
        guard.markDirty();
    }
    (void)bpm_->flushPage(table_info->getHeapRootPage());
    
    TableHeap heap = *TableHeap::open(*disk_manager_, table_info->getHeapRootPage());
    RID r1, r2;
    (void)heap.insertTuple(Tuple(payload1), r1);
    (void)heap.insertTuple(Tuple(payload2), r2);
    txn_manager_->commit(txn);
    
    // Predicate: true
    auto true_expr_factory = []() {
        return std::make_unique<hamdb::ConstantExpression>(Value(true));
    };

    // Unoptimized Plan
    auto unopt_seq = std::make_unique<planner::SeqScanPlanNode>(table_info->getSchema(), "test_table", "test_table");
    auto unopt_filter = std::make_unique<planner::FilterPlanNode>(table_info->getSchema(), true_expr_factory());
    unopt_filter->addChild(std::move(unopt_seq));
    auto unopt_phys = physical_planner_->plan(std::move(unopt_filter));
    auto unopt_exec = planner::ExecutorFactory::createExecutor(&exec_ctx, std::move(unopt_phys));
    
    unopt_exec->init();
    Tuple t_unopt;
    RID r_unopt;
    int unopt_count = 0;
    while(unopt_exec->next(&t_unopt, &r_unopt)) {
        unopt_count++;
    }
    EXPECT_EQ(unopt_count, 2);
    
    // Optimized Plan
    auto opt_seq = std::make_unique<planner::SeqScanPlanNode>(table_info->getSchema(), "test_table", "test_table");
    auto opt_filter = std::make_unique<planner::FilterPlanNode>(table_info->getSchema(), true_expr_factory());
    opt_filter->addChild(std::move(opt_seq));
    
    std::unique_ptr<planner::LogicalPlanNode> opt_root = std::move(opt_filter);
    opt_root = rule_executor_->optimize(std::move(opt_root));
    
    auto opt_phys = physical_planner_->plan(std::move(opt_root));
    auto opt_exec = planner::ExecutorFactory::createExecutor(&exec_ctx, std::move(opt_phys));
    
    opt_exec->init();
    Tuple t_opt;
    RID r_opt;
    int opt_count = 0;
    while(opt_exec->next(&t_opt, &r_opt)) {
        opt_count++;
    }
    EXPECT_EQ(opt_count, 2);
}

} // namespace hamdb::optimizer
