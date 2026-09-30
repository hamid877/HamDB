#include "optimizer/rule_executor.hpp"
#include "optimizer/top_k_optimization_rule.hpp"
#include "planner/logical_plan.hpp"
#include "planner/top_k_plan.hpp"
#include "planner/order_by_plan.hpp"
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
#include <random>
#include <algorithm>
#include "utils/serializer.hpp"

namespace hamdb::optimizer {

class TopKOptimizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_ = "test_top_k_optimization.hamdb";
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
        rule_executor_->addRule(std::make_unique<TopKOptimizationRule>());

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

TEST_F(TopKOptimizationTest, StructuralRewriteTest) {
    TableInfo* table_info;
    (void)catalog_->getTable("test_table", table_info);
    
    auto seq_scan = std::make_unique<planner::SeqScanPlanNode>(table_info->getSchema(), "test_table", "test_table", nullptr);
    
    std::vector<std::pair<std::unique_ptr<hamdb::Expression>, bool>> order_by;
    order_by.emplace_back(std::make_unique<hamdb::ColumnValueExpression>(0), true); // ASC on col 0
    
    auto logical_order_by = std::make_unique<planner::LogicalOrderByNode>(table_info->getSchema(), std::move(order_by));
    logical_order_by->addChild(std::move(seq_scan));
    
    auto limit = std::make_unique<planner::LimitPlanNode>(table_info->getSchema(), std::make_unique<hamdb::ConstantExpression>(Value(10)), std::make_unique<hamdb::ConstantExpression>(Value(0)));
    limit->addChild(std::move(logical_order_by));
    
    std::unique_ptr<planner::LogicalPlanNode> root = std::move(limit);
    auto optimized_root = rule_executor_->optimize(std::move(root));
    
    ASSERT_EQ(optimized_root->getType(), planner::LogicalPlanType::TOP_K);
    auto* top_k_node = static_cast<planner::LogicalTopKNode*>(optimized_root.get());
    
    ASSERT_NE(top_k_node->getLimit(), nullptr);
    ASSERT_EQ(top_k_node->getLimit()->evaluate(Tuple{}, Schema(std::vector<Column>{})).getAsInteger(), 10);
    ASSERT_EQ(top_k_node->getChildren().size(), 1);
    ASSERT_EQ(top_k_node->getChildren()[0]->getType(), planner::LogicalPlanType::SEQ_SCAN);
}

TEST_F(TopKOptimizationTest, DifferentialExecutionTest) {
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

    std::mt19937 gen(42);
    std::uniform_int_distribution<> dis(1, 1000);
    for (int i = 0; i < 200; i++) {
        RID r;
        (void)heap.insertTuple(insert_tuple(dis(gen), i), r);
    }
    txn_manager_->commit(txn);
    
    // Unoptimized Plan: LIMIT -> ORDER BY
    auto unoptimized_scan = std::make_unique<planner::SeqScanPlanNode>(table_info->getSchema(), "test_table", "test_table", nullptr);
    std::vector<std::pair<std::unique_ptr<hamdb::Expression>, bool>> order_by_u;
    order_by_u.emplace_back(std::make_unique<hamdb::ColumnValueExpression>(0), true);
    auto logical_order_by_u = std::make_unique<planner::LogicalOrderByNode>(table_info->getSchema(), std::move(order_by_u));
    logical_order_by_u->addChild(std::move(unoptimized_scan));
    auto limit_u = std::make_unique<planner::LimitPlanNode>(table_info->getSchema(), std::make_unique<hamdb::ConstantExpression>(Value(10)), std::make_unique<hamdb::ConstantExpression>(Value(5)));
    limit_u->addChild(std::move(logical_order_by_u));
    
    auto phys_root_u = physical_planner_->plan(std::move(limit_u));
    auto exec_u = planner::ExecutorFactory::createExecutor(&exec_ctx, std::move(phys_root_u));
    
    exec_u->init();
    std::vector<Tuple> unoptimized_res;
    Tuple t_u;
    RID tr_u;
    while(exec_u->next(&t_u, &tr_u)) {
        unoptimized_res.push_back(t_u);
    }
    
    // Optimized Plan: TOP_K
    auto optimized_scan = std::make_unique<planner::SeqScanPlanNode>(table_info->getSchema(), "test_table", "test_table", nullptr);
    std::vector<std::pair<std::unique_ptr<hamdb::Expression>, bool>> order_by_o;
    order_by_o.emplace_back(std::make_unique<hamdb::ColumnValueExpression>(0), true);
    auto logical_order_by_o = std::make_unique<planner::LogicalOrderByNode>(table_info->getSchema(), std::move(order_by_o));
    logical_order_by_o->addChild(std::move(optimized_scan));
    auto limit_o = std::make_unique<planner::LimitPlanNode>(table_info->getSchema(), std::make_unique<hamdb::ConstantExpression>(Value(10)), std::make_unique<hamdb::ConstantExpression>(Value(5)));
    limit_o->addChild(std::move(logical_order_by_o));
    
    auto opt_root = rule_executor_->optimize(std::move(limit_o));
    auto phys_root_o = physical_planner_->plan(std::move(opt_root));
    auto exec_o = planner::ExecutorFactory::createExecutor(&exec_ctx, std::move(phys_root_o));
    
    exec_o->init();
    std::vector<Tuple> optimized_res;
    Tuple t_o;
    RID tr_o;
    while(exec_o->next(&t_o, &tr_o)) {
        optimized_res.push_back(t_o);
    }
    
    ASSERT_EQ(unoptimized_res.size(), optimized_res.size());
    for (size_t i = 0; i < unoptimized_res.size(); i++) {
        auto val_u = hamdb::ColumnValueExpression(0).evaluate(unoptimized_res[i], table_info->getSchema());
        auto val_o = hamdb::ColumnValueExpression(0).evaluate(optimized_res[i], table_info->getSchema());
        EXPECT_EQ(val_u.getAsInteger(), val_o.getAsInteger());
    }
}

} // namespace hamdb::optimizer
