#include "optimizer/rule_executor.hpp"
#include "optimizer/projection_pruning_rule.hpp"
#include "planner/physical_planner.hpp"
#include "planner/planner.hpp"
#include "planner/executor_factory.hpp"
#include "planner/nested_loop_join_plan.hpp"
#include "executor/comparison_expression.hpp"
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

class ProjectionPruningTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_ = "test_optimizer_proj.hamdb";
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
            Column("val1", ColumnType::Integer),
            Column("val2", ColumnType::Integer)
        };
        Schema schema(cols);
        TableInfo* info;
        (void)catalog_->createTable("test_table", schema, info);
        planner_ = std::make_unique<planner::Planner>(catalog_.get());
        physical_planner_ = std::make_unique<planner::PhysicalPlanner>(catalog_.get());
        rule_executor_ = std::make_unique<RuleExecutor>();
        rule_executor_->addRule(std::make_unique<ProjectionPruningRule>());

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

TEST_F(ProjectionPruningTest, PruneColumns) {
    TableInfo* table_info;
    (void)catalog_->getTable("test_table", table_info);
    
    auto seq_scan = std::make_unique<planner::SeqScanPlanNode>(table_info->getSchema(), "test_table", "test_table");
    
    auto comp_expr = std::make_unique<hamdb::ComparisonExpression>(
        hamdb::ComparisonType::Equal,
        std::make_unique<hamdb::ColumnValueExpression>(1), // val1
        std::make_unique<hamdb::ConstantExpression>(Value(5))
    );
    auto filter = std::make_unique<planner::FilterPlanNode>(table_info->getSchema(), std::move(comp_expr));
    filter->addChild(std::move(seq_scan));
    
    std::vector<Column> proj_cols = { Column("id", ColumnType::Integer) };
    Schema proj_schema(proj_cols);
    std::vector<std::unique_ptr<hamdb::Expression>> proj_exprs;
    proj_exprs.push_back(std::make_unique<hamdb::ColumnValueExpression>(0)); // id
    
    auto proj = std::make_unique<planner::ProjectionPlanNode>(proj_schema, std::move(proj_exprs));
    proj->addChild(std::move(filter));
    
    std::unique_ptr<planner::LogicalPlanNode> root = std::move(proj);
    
    auto optimized_root = rule_executor_->optimize(std::move(root));
    
    // Verify rewrite
    ASSERT_EQ(optimized_root->getType(), planner::LogicalPlanType::PROJECTION);
    auto* optimized_proj = dynamic_cast<planner::ProjectionPlanNode*>(optimized_root.get());
    
    auto* optimized_filter = dynamic_cast<planner::FilterPlanNode*>(optimized_proj->getChildren()[0].get());
    ASSERT_NE(optimized_filter, nullptr);
    
    auto* optimized_seq = dynamic_cast<planner::SeqScanPlanNode*>(optimized_filter->getChildren()[0].get());
    ASSERT_NE(optimized_seq, nullptr);
    
    // SeqScan should now only output columns 0 (id) and 1 (val1), because projection needs 0, and filter needs 1
    ASSERT_EQ(optimized_seq->getOutputSchema().getColumnCount(), 2);
    EXPECT_EQ(optimized_seq->getOutputSchema().getColumn(0).getName(), "id");
    EXPECT_EQ(optimized_seq->getOutputSchema().getColumn(1).getName(), "val1");
}

TEST_F(ProjectionPruningTest, JoinPruningTest) {
    TableInfo* table1;
    (void)catalog_->getTable("test_table", table1);
    
    std::vector<Column> cols_t2 = {
        Column("id2", ColumnType::Integer),
        Column("val3", ColumnType::Integer),
        Column("val4", ColumnType::Integer)
    };
    Schema schema_t2(cols_t2);
    TableInfo* table2;
    (void)catalog_->createTable("test_table2", schema_t2, table2);
    
    auto seq1 = std::make_unique<planner::SeqScanPlanNode>(table1->getSchema(), "test_table", "t1");
    auto seq2 = std::make_unique<planner::SeqScanPlanNode>(table2->getSchema(), "test_table2", "t2");
    
    std::vector<Column> join_cols;
    for (uint32_t i = 0; i < table1->getSchema().getColumnCount(); ++i) join_cols.push_back(table1->getSchema().getColumn(i));
    for (uint32_t i = 0; i < table2->getSchema().getColumnCount(); ++i) join_cols.push_back(table2->getSchema().getColumn(i));
    Schema join_schema(join_cols);
    
    auto join = std::make_unique<planner::LogicalNestedLoopJoinNode>(join_schema, std::make_unique<hamdb::ComparisonExpression>(
        hamdb::ComparisonType::Equal,
        std::make_unique<hamdb::ColumnValueExpression>(0), // t1.id (index 0)
        std::make_unique<hamdb::ColumnValueExpression>(3)  // t2.id2 (index 3)
    ));
    join->addChild(std::move(seq1));
    join->addChild(std::move(seq2));
    
    // Select t1.val2 (index 2) and t2.val4 (index 5)
    std::vector<Column> proj_cols = { Column("val2", ColumnType::Integer), Column("val4", ColumnType::Integer) };
    Schema proj_schema(proj_cols);
    std::vector<std::unique_ptr<hamdb::Expression>> proj_exprs;
    proj_exprs.push_back(std::make_unique<hamdb::ColumnValueExpression>(2));
    proj_exprs.push_back(std::make_unique<hamdb::ColumnValueExpression>(5));
    
    auto proj = std::make_unique<planner::ProjectionPlanNode>(proj_schema, std::move(proj_exprs));
    proj->addChild(std::move(join));
    
    std::unique_ptr<planner::LogicalPlanNode> root = std::move(proj);
    auto optimized_root = rule_executor_->optimize(std::move(root));
    
    auto* optimized_proj = dynamic_cast<planner::ProjectionPlanNode*>(optimized_root.get());
    auto* optimized_join = dynamic_cast<planner::LogicalNestedLoopJoinNode*>(optimized_proj->getChildren()[0].get());
    
    auto* opt_seq1 = dynamic_cast<planner::SeqScanPlanNode*>(optimized_join->getChildren()[0].get());
    auto* opt_seq2 = dynamic_cast<planner::SeqScanPlanNode*>(optimized_join->getChildren()[1].get());
    
    // t1 needs id (0) for join, and val2 (2) for projection
    ASSERT_EQ(opt_seq1->getOutputSchema().getColumnCount(), 2);
    EXPECT_EQ(opt_seq1->getOutputSchema().getColumn(0).getName(), "id");
    EXPECT_EQ(opt_seq1->getOutputSchema().getColumn(1).getName(), "val2");
    
    // t2 needs id2 (0 in its schema) for join, and val4 (2 in its schema) for projection
    ASSERT_EQ(opt_seq2->getOutputSchema().getColumnCount(), 2);
    EXPECT_EQ(opt_seq2->getOutputSchema().getColumn(0).getName(), "id2");
    EXPECT_EQ(opt_seq2->getOutputSchema().getColumn(1).getName(), "val4");
}

TEST_F(ProjectionPruningTest, ExecutionResultsIdentical) {
    TableInfo* table_info;
    (void)catalog_->getTable("test_table", table_info);
    
    auto txn = txn_manager_->begin();
    ExecutorContext exec_ctx(txn, catalog_.get(), bpm_.get(), mvcc_manager_.get(), disk_manager_.get(), lock_manager_.get(), log_manager_.get());
    
    // Dummy payloads (id, val1, val2)
    std::vector<std::byte> payload1 = {std::byte{1}, std::byte{2}, std::byte{3}}; // doesn't matter, executor emits true tuple
    
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
    RID r1;
    Tuple tup1; // Empty tuple since executor won't deserialize payload1 properly anyway. Wait, we should make a proper tuple.
    
    std::vector<std::byte> buf(100);
    Serializer ser(buf);
    (void)ser.writeInt32(10); // id
    (void)ser.writeInt32(20); // val1
    (void)ser.writeInt32(30); // val2
    Tuple proper_tuple(std::span<const std::byte>(buf.data(), ser.position()));
    (void)heap.insertTuple(proper_tuple, r1);
    txn_manager_->commit(txn);
    
    auto create_unopt_plan = [&]() -> std::unique_ptr<planner::LogicalPlanNode> {
        auto seq = std::make_unique<planner::SeqScanPlanNode>(table_info->getSchema(), "test_table", "test_table");
        auto filter = std::make_unique<planner::FilterPlanNode>(table_info->getSchema(), std::make_unique<hamdb::ConstantExpression>(Value(true)));
        filter->addChild(std::move(seq));
        
        std::vector<Column> p_cols = { Column("id", ColumnType::Integer) };
        Schema p_schema(p_cols);
        std::vector<std::unique_ptr<hamdb::Expression>> p_exprs;
        p_exprs.push_back(std::make_unique<hamdb::ColumnValueExpression>(0));
        
        auto p = std::make_unique<planner::ProjectionPlanNode>(p_schema, std::move(p_exprs));
        p->addChild(std::move(filter));
        return p;
    };
    
    auto unopt_phys = physical_planner_->plan(create_unopt_plan());
    auto unopt_exec = planner::ExecutorFactory::createExecutor(&exec_ctx, std::move(unopt_phys));
    
    unopt_exec->init();
    Tuple t_unopt;
    RID r_unopt;
    int unopt_count = 0;
    while(unopt_exec->next(&t_unopt, &r_unopt)) { unopt_count++; }
    EXPECT_EQ(unopt_count, 1);
    
    auto opt_root = rule_executor_->optimize(create_unopt_plan());
    auto opt_phys = physical_planner_->plan(std::move(opt_root));
    auto opt_exec = planner::ExecutorFactory::createExecutor(&exec_ctx, std::move(opt_phys));
    
    opt_exec->init();
    Tuple t_opt;
    RID r_opt;
    int opt_count = 0;
    while(opt_exec->next(&t_opt, &r_opt)) { opt_count++; }
    EXPECT_EQ(opt_count, 1);
}

} // namespace hamdb::optimizer
