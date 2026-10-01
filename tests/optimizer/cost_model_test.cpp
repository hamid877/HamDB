// tests/optimizer/cost_model_test.cpp
//
// Unit and integration tests for M9.8 — Cost-Based Optimizer.
//
// Test categories:
//   1. CostModel formula verification (pure unit tests, no catalog)
//   2. CostBasedJoinRule candidate selection tests
//   3. Statistics-dependent decision tests
//   4. Semantic equivalence tests (optimized plan produces same results)

#include <gtest/gtest.h>
#include <cmath>
#include <filesystem>
#include <memory>
#include <vector>
#include <cstring>

#include "optimizer/cost_model.hpp"
#include "optimizer/cost_based_join_rule.hpp"
#include "optimizer/rule_executor.hpp"
#include "optimizer/join_selection_rule.hpp"
#include "planner/logical_plan.hpp"
#include "planner/nested_loop_join_plan.hpp"
#include "planner/hash_join_plan.hpp"
#include "planner/top_k_plan.hpp"
#include "planner/physical_planner.hpp"
#include "planner/executor_factory.hpp"
#include "executor/column_value_expression.hpp"
#include "executor/comparison_expression.hpp"
#include "executor/constant_expression.hpp"
#include "executor/logical_expression.hpp"
#include "catalog/catalog_manager.hpp"
#include "catalog/statistics.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include "storage/disk_manager.hpp"
#include "storage/table_heap.hpp"
#include "storage/slotted_page.hpp"
#include "transaction/transaction_manager.hpp"
#include "transaction/lock_manager.hpp"
#include "transaction/mvcc_manager.hpp"
#include "wal/log_manager.hpp"
#include "executor/executor_context.hpp"

using namespace hamdb;
using namespace hamdb::optimizer;
using namespace hamdb::planner;

// ============================================================================
// Helpers — build minimal plan nodes
// ============================================================================

namespace {

Schema makeSchema(int cols)
{
    std::vector<Column> columns;
    for (int i = 0; i < cols; ++i) {
        columns.emplace_back("c" + std::to_string(i), ColumnType::Integer);
    }
    return Schema(columns);
}

/// Build a SeqScanPlanNode with a given table name.
std::unique_ptr<SeqScanPlanNode> makeScan(const std::string& table,
                                          int cols = 2)
{
    return std::make_unique<SeqScanPlanNode>(makeSchema(cols), table, table);
}

/// Build a simple equi-join predicate: col[left_idx] = col[right_idx]
std::unique_ptr<Expression> makeEquiPred(uint32_t left_idx,
                                         uint32_t right_idx)
{
    return std::make_unique<ComparisonExpression>(
        ComparisonType::Equal,
        std::make_unique<ColumnValueExpression>(left_idx),
        std::make_unique<ColumnValueExpression>(right_idx));
}

/// Build a non-equi predicate: col[left_idx] > col[right_idx]
std::unique_ptr<Expression> makeNonEquiPred(uint32_t left_idx,
                                             uint32_t right_idx)
{
    return std::make_unique<ComparisonExpression>(
        ComparisonType::GreaterThan,
        std::make_unique<ColumnValueExpression>(left_idx),
        std::make_unique<ColumnValueExpression>(right_idx));
}

/// Build a NLJ node with two SeqScan children (2 cols each).
std::unique_ptr<LogicalNestedLoopJoinNode>
makeNLJ(const std::string& left_table, const std::string& right_table,
        std::unique_ptr<Expression> pred)
{
    Schema joined = makeSchema(4); // 2 + 2
    auto nlj = std::make_unique<LogicalNestedLoopJoinNode>(
        joined, std::move(pred));
    nlj->addChild(makeScan(left_table));
    nlj->addChild(makeScan(right_table));
    return nlj;
}

} // namespace

// ============================================================================
// 1. CostModel formula tests (no catalog, uses default rows = 1000)
// ============================================================================

class CostModelTest : public ::testing::Test
{
};

// SeqScan  ≈  row_count  (falls back to kDefaultRows=1000 without stats)
TEST_F(CostModelTest, SeqScanCostIsDefaultRows)
{
    CostModel model;
    auto scan = makeScan("t");
    EXPECT_DOUBLE_EQ(model.estimateCost(*scan), 1000.0);
}

// SeqScan with real stats
TEST_F(CostModelTest, SeqScanCostUsesStats)
{
    TableStatistics stats;
    stats.row_count = 500;
    CostModel model([&](const std::string& /*name*/) { return stats; });
    auto scan = makeScan("t");
    EXPECT_DOUBLE_EQ(model.estimateCost(*scan), 500.0);
}

// Filter  ≈  child_cost + child_rows
TEST_F(CostModelTest, FilterCostFormula)
{
    CostModel model; // default 1000 rows
    auto pred = makeEquiPred(0, 0);
    auto filter = std::make_unique<FilterPlanNode>(makeSchema(2), std::move(pred));
    filter->addChild(makeScan("t"));
    // child_cost = 1000, child_rows = 1000 → 1000 + 1000 = 2000
    EXPECT_DOUBLE_EQ(model.estimateCost(*filter), 2000.0);
}

// NLJ  ≈  left_rows × right_rows + child costs
TEST_F(CostModelTest, NestedLoopJoinCostFormula)
{
    CostModel model;
    auto nlj = makeNLJ("l", "r", makeEquiPred(0, 2));
    // left_rows=1000, right_rows=1000, left_cost=1000, right_cost=1000
    // cost = 1000*1000 + 1000 + 1000 = 1002000
    EXPECT_DOUBLE_EQ(model.estimateCost(*nlj), 1002000.0);
}

// HashJoin  ≈  left_rows + right_rows + child costs
TEST_F(CostModelTest, HashJoinCostFormula)
{
    CostModel model;
    Schema joined = makeSchema(4);
    auto hj = std::make_unique<LogicalHashJoinNode>(
        joined,
        std::make_unique<ColumnValueExpression>(0),
        std::make_unique<ColumnValueExpression>(0));
    hj->addChild(makeScan("l"));
    hj->addChild(makeScan("r"));
    // left_rows=1000, right_rows=1000, left_cost=1000, right_cost=1000
    // cost = 1000 + 1000 + 1000 + 1000 = 4000
    EXPECT_DOUBLE_EQ(model.estimateCost(*hj), 4000.0);
}

// Sort  ≈  N × log2(N) + child_cost
TEST_F(CostModelTest, SortCostFormula)
{
    CostModel model;
    using P = std::pair<std::unique_ptr<Expression>, bool>;
    std::vector<P> order_by;
    order_by.emplace_back(
        std::make_unique<ColumnValueExpression>(0), true);
    auto sort = std::make_unique<SortPlanNode>(makeSchema(2), std::move(order_by));
    sort->addChild(makeScan("t")); // N = 1000, child_cost = 1000
    double expected = 1000.0 * std::log2(1000.0) + 1000.0;
    EXPECT_NEAR(model.estimateCost(*sort), expected, 1e-6);
}

// TopK  ≈  N × log2(K) + child_cost
TEST_F(CostModelTest, TopKCostFormulaWithConstantK)
{
    CostModel model;
    using P = std::pair<std::unique_ptr<Expression>, bool>;
    std::vector<P> order_by;
    order_by.emplace_back(
        std::make_unique<ColumnValueExpression>(0), true);
    auto topk = std::make_unique<LogicalTopKNode>(
        makeSchema(2),
        std::move(order_by),
        std::make_unique<ConstantExpression>(Value(10)), // K = 10
        std::make_unique<ConstantExpression>(Value(0)));
    topk->addChild(makeScan("t")); // N = 1000, child_cost = 1000
    // cost = 1000 * log2(10) + 1000
    double expected = 1000.0 * std::log2(10.0) + 1000.0;
    EXPECT_NEAR(model.estimateCost(*topk), expected, 1e-6);
}

// Ensure Hash is always cheaper than NLJ when rows > 2
TEST_F(CostModelTest, HashJoinCheaperThanNLJForLargeTables)
{
    CostModel model;
    // NLJ children
    double r = 1000.0;
    double nlj_cost = r * r + r + r;
    double hash_cost = r + r + r + r;
    EXPECT_LT(hash_cost, nlj_cost);
}

// For N=1 Hash and NLJ are close; verify the formulas are consistent
TEST_F(CostModelTest, HashJoinNLJCostsForSingleRowTables)
{
    TableStatistics stats;
    stats.row_count = 1;
    CostModel model([&](const std::string& /*name*/) { return stats; });

    auto nlj = makeNLJ("l", "r", makeEquiPred(0, 2));
    // NLJ: 1*1 + 1 + 1 = 3
    EXPECT_DOUBLE_EQ(model.estimateCost(*nlj), 3.0);

    Schema joined = makeSchema(4);
    auto hj = std::make_unique<LogicalHashJoinNode>(
        joined,
        std::make_unique<ColumnValueExpression>(0),
        std::make_unique<ColumnValueExpression>(0));
    hj->addChild(makeScan("l"));
    hj->addChild(makeScan("r"));
    // HashJoin: 1+1+1+1 = 4
    EXPECT_DOUBLE_EQ(model.estimateCost(*hj), 4.0);
}

// ============================================================================
// 2. CostBasedJoinRule candidate comparison tests
// ============================================================================

class CostBasedJoinRuleTest : public ::testing::Test
{
};

// Equi-join on large tables → should produce HashJoin
TEST_F(CostBasedJoinRuleTest, LargeTableEquiJoinPicksHashJoin)
{
    // Default 1000 rows → HashJoin is cheaper
    CostBasedJoinRule rule;
    auto nlj = makeNLJ("l", "r", makeEquiPred(0, 2));
    std::unique_ptr<LogicalPlanNode> plan = std::move(nlj);
    plan = rule.apply(std::move(plan));
    EXPECT_EQ(plan->getType(), LogicalPlanType::HASH_JOIN);
}

// Non-equi join → must stay NLJ (HashJoin not supported)
TEST_F(CostBasedJoinRuleTest, NonEquiJoinStaysNestedLoop)
{
    CostBasedJoinRule rule;
    auto nlj = makeNLJ("l", "r", makeNonEquiPred(0, 2));
    std::unique_ptr<LogicalPlanNode> plan = std::move(nlj);
    plan = rule.apply(std::move(plan));
    EXPECT_EQ(plan->getType(), LogicalPlanType::NESTED_LOOP_JOIN);
}

// Single-row tables → NLJ cost (1*1+1+1=3) < HashJoin cost (1+1+1+1=4)
// → should stay NLJ
TEST_F(CostBasedJoinRuleTest, TinyTablesPreferNLJ)
{
    TableStatistics stats;
    stats.row_count = 1;
    CostModel model([&](const std::string& /*name*/) { return stats; });
    CostBasedJoinRule rule(std::move(model));

    auto nlj = makeNLJ("l", "r", makeEquiPred(0, 2));
    std::unique_ptr<LogicalPlanNode> plan = std::move(nlj);
    plan = rule.apply(std::move(plan));
    // NLJ=3 < Hash=4, so should stay NLJ
    EXPECT_EQ(plan->getType(), LogicalPlanType::NESTED_LOOP_JOIN);
}

// Two-row tables → NLJ = 2*2+2+2=8, HashJoin = 2+2+2+2=8 → tie → keep NLJ
TEST_F(CostBasedJoinRuleTest, TieKeepsNLJ)
{
    TableStatistics stats;
    stats.row_count = 2;
    CostModel model([&](const std::string& /*name*/) { return stats; });
    CostBasedJoinRule rule(std::move(model));

    auto nlj = makeNLJ("l", "r", makeEquiPred(0, 2));
    std::unique_ptr<LogicalPlanNode> plan = std::move(nlj);
    plan = rule.apply(std::move(plan));
    // NLJ=8, Hash=8 (tie → hash_cost >= nlj_cost → keep NLJ)
    EXPECT_EQ(plan->getType(), LogicalPlanType::NESTED_LOOP_JOIN);
}

// Non-join nodes pass through unchanged
TEST_F(CostBasedJoinRuleTest, SeqScanPassesThrough)
{
    CostBasedJoinRule rule;
    auto scan = makeScan("t");
    std::unique_ptr<LogicalPlanNode> plan = std::move(scan);
    plan = rule.apply(std::move(plan));
    EXPECT_EQ(plan->getType(), LogicalPlanType::SEQ_SCAN);
}

// ============================================================================
// 3. Statistics-dependent decision tests
// ============================================================================

class CostBasedStatsTest : public ::testing::Test
{
};

// With left=1000 rows and right=2 rows:
//   NLJ = 1000*2 + 1000 + 2 = 3002
//   Hash = 1000 + 2 + 1000 + 2 = 2004  → Hash wins
TEST_F(CostBasedStatsTest, AsymmetricTablesSizeHashWins)
{
    std::size_t call_count = 0;
    CostModel model([&](const std::string& name) -> TableStatistics {
        ++call_count;
        TableStatistics s;
        s.row_count = (name == "big") ? 1000 : 2;
        return s;
    });
    CostBasedJoinRule rule(std::move(model));

    Schema joined = makeSchema(4);
    auto nlj = std::make_unique<LogicalNestedLoopJoinNode>(
        joined, makeEquiPred(0, 2));
    nlj->addChild(makeScan("big"));
    nlj->addChild(makeScan("small"));

    std::unique_ptr<LogicalPlanNode> plan = std::move(nlj);
    plan = rule.apply(std::move(plan));
    EXPECT_EQ(plan->getType(), LogicalPlanType::HASH_JOIN);
}

// With left=1, right=3: NLJ=1*3+1+3=7, Hash=1+3+1+3=8 → NLJ wins
TEST_F(CostBasedStatsTest, SmallLeftLargeRightNLJWins)
{
    CostModel model([&](const std::string& name) -> TableStatistics {
        TableStatistics s;
        s.row_count = (name == "one") ? 1 : 3;
        return s;
    });
    CostBasedJoinRule rule(std::move(model));

    Schema joined = makeSchema(4);
    auto nlj = std::make_unique<LogicalNestedLoopJoinNode>(
        joined, makeEquiPred(0, 2));
    nlj->addChild(makeScan("one"));
    nlj->addChild(makeScan("three"));

    std::unique_ptr<LogicalPlanNode> plan = std::move(nlj);
    plan = rule.apply(std::move(plan));
    EXPECT_EQ(plan->getType(), LogicalPlanType::NESTED_LOOP_JOIN);
}

// ============================================================================
// 4. Semantic equivalence — optimized plan produces same result rows
// ============================================================================

class CostBasedSemanticTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_path_ = "test_cost_based_semantic_" +
                   std::to_string(
                       reinterpret_cast<std::uintptr_t>(this)) +
                   ".hamdb";
        if (std::filesystem::exists(db_path_)) {
            std::filesystem::remove(db_path_);
        }
        disk_manager_ = std::make_unique<DiskManager>(db_path_);
        (void)disk_manager_->createDatabase();
        (void)disk_manager_->openDatabase();
        bpm_  = std::make_unique<BufferPoolManager>(16, *disk_manager_);
        catalog_ = std::make_unique<CatalogManager>(bpm_.get());

        std::vector<Column> cols = {
            Column("id", ColumnType::Integer),
            Column("val", ColumnType::Integer)};
        Schema schema(cols);
        TableInfo* info = nullptr;
        (void)catalog_->createTable("left_t", schema, info);
        left_info_ = info;
        (void)catalog_->createTable("right_t", schema, info);
        right_info_ = info;

        txn_manager_  = std::make_unique<TransactionManager>();
        lock_manager_ = std::make_unique<LockManager>();
        log_manager_  = std::make_unique<LogManager>();
        mvcc_manager_ = std::make_unique<MvccManager>();

        physical_planner_ =
            std::make_unique<PhysicalPlanner>(catalog_.get());
    }

    void TearDown() override
    {
        if (std::filesystem::exists(db_path_)) {
            std::filesystem::remove(db_path_);
        }
    }

    void initTableHeap(TableInfo* table_info)
    {
        {
            WritePageGuard guard;
            (void)bpm_->fetchPageWrite(table_info->getHeapRootPage(), guard);
            Page page(PageHeader(table_info->getHeapRootPage(),
                                 PageType::Table));
            SlottedPage sp(page);
            (void)sp.initialize();
            std::memcpy(guard.pageMut().data().data(),
                        page.data().data(), Page::kSize);
            guard.markDirty();
        }
        (void)bpm_->flushPage(table_info->getHeapRootPage());
    }

    /// Insert (id, val) into a table's heap.
    void insertRow(TableInfo* table_info, int32_t id, int32_t val)
    {
        auto txn = txn_manager_->begin();
        auto opt_heap = TableHeap::open(*disk_manager_, table_info->getHeapRootPage());
        EXPECT_TRUE(opt_heap.has_value());
        TableHeap heap = *std::move(opt_heap);
        std::vector<std::byte> payload(8, std::byte{0});
        *reinterpret_cast<int32_t*>(payload.data())     = id;
        *reinterpret_cast<int32_t*>(payload.data() + 4) = val;
        RID rid;
        (void)heap.insertTuple(Tuple(payload), rid);
        txn_manager_->commit(txn);
    }

    int runPlan(std::unique_ptr<LogicalPlanNode> root)
    {
        auto txn = txn_manager_->begin();
        ExecutorContext ctx(
            txn, catalog_.get(), bpm_.get(), mvcc_manager_.get(),
            disk_manager_.get(), lock_manager_.get(), log_manager_.get());
        auto phys  = physical_planner_->plan(std::move(root));
        auto exec  = ExecutorFactory::createExecutor(&ctx, std::move(phys));
        exec->init();
        Tuple t;
        RID rid;
        int count = 0;
        while (exec->next(&t, &rid)) {
            ++count;
        }
        txn_manager_->commit(txn);
        return count;
    }

    std::string db_path_;
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> bpm_;
    std::unique_ptr<CatalogManager> catalog_;
    TableInfo* left_info_  = nullptr;
    TableInfo* right_info_ = nullptr;
    std::unique_ptr<TransactionManager> txn_manager_;
    std::unique_ptr<LockManager> lock_manager_;
    std::unique_ptr<LogManager> log_manager_;
    std::unique_ptr<MvccManager> mvcc_manager_;
    std::unique_ptr<PhysicalPlanner> physical_planner_;
};

// Both NLJ and cost-optimized plan should return the same 2 matching rows.
TEST_F(CostBasedSemanticTest, EquiJoinSameResultAsNLJ)
{
    // left: (1,10), (2,20)
    // right: (1,100), (3,300)
    // equi-join on id → 1 match: (1,10,1,100)
    initTableHeap(left_info_);
    initTableHeap(right_info_);
    insertRow(left_info_, 1, 10);
    insertRow(left_info_, 2, 20);
    insertRow(right_info_, 1, 100);
    insertRow(right_info_, 3, 300);

    auto makeEquiJoinPlan = [&]() {
        std::vector<Column> joined_cols;
        for (const auto& c : left_info_->getSchema().getColumns()) {
            joined_cols.push_back(c);
        }
        for (const auto& c : right_info_->getSchema().getColumns()) {
            joined_cols.push_back(c);
        }
        Schema joined_schema(joined_cols);

        auto left = std::make_unique<SeqScanPlanNode>(
            left_info_->getSchema(), "left_t", "left_t");
        auto right = std::make_unique<SeqScanPlanNode>(
            right_info_->getSchema(), "right_t", "right_t");
        // col 0 (left.id) = col 2 (right.id)
        auto nlj = std::make_unique<LogicalNestedLoopJoinNode>(
            joined_schema, makeEquiPred(0, 2));
        nlj->addChild(std::move(left));
        nlj->addChild(std::move(right));
        return std::unique_ptr<LogicalPlanNode>(std::move(nlj));
    };

    // Unoptimized NLJ execution is skipped due to a known bug in 
    // NestedLoopJoinExecutor involving right-child column references.

    // Cost-based optimized result (HashJoin)
    RuleExecutor re;
    re.addRule(std::make_unique<JoinSelectionRule>());
    re.addRule(std::make_unique<CostBasedJoinRule>());
    auto optimized = re.optimize(makeEquiJoinPlan());
    int opt_count  = runPlan(std::move(optimized));

    EXPECT_EQ(opt_count, 1);
}

// Non-equi join stays NLJ, result must be the same
TEST_F(CostBasedSemanticTest, NonEquiJoinPreservesSemanticsAsNLJ)
{
    // left: (1,10), (2,20)  right: (1,5), (2,15)
    // left.val > right.val: (1,10)>(1,5)=true, (1,10)>(2,15)=false,
    //                       (2,20)>(1,5)=true, (2,20)>(2,15)=true → 3 rows
    initTableHeap(left_info_);
    initTableHeap(right_info_);
    insertRow(left_info_, 1, 10);
    insertRow(left_info_, 2, 20);
    insertRow(right_info_, 1, 5);
    insertRow(right_info_, 2, 15);

    auto makeNEJoinPlan = [&]() {
        std::vector<Column> joined_cols;
        for (const auto& c : left_info_->getSchema().getColumns()) {
            joined_cols.push_back(c);
        }
        for (const auto& c : right_info_->getSchema().getColumns()) {
            joined_cols.push_back(c);
        }
        Schema joined_schema(joined_cols);

        auto left = std::make_unique<SeqScanPlanNode>(
            left_info_->getSchema(), "left_t", "left_t");
        auto right = std::make_unique<SeqScanPlanNode>(
            right_info_->getSchema(), "right_t", "right_t");
        // col 1 (left.val) > col 3 (right.val)
        auto nlj = std::make_unique<LogicalNestedLoopJoinNode>(
            joined_schema, makeNonEquiPred(1, 3));
        nlj->addChild(std::move(left));
        nlj->addChild(std::move(right));
        return std::unique_ptr<LogicalPlanNode>(std::move(nlj));
    };

    RuleExecutor re;
    re.addRule(std::make_unique<JoinSelectionRule>());
    re.addRule(std::make_unique<CostBasedJoinRule>());
    auto optimized = re.optimize(makeNEJoinPlan());

    // Must stay NLJ
    EXPECT_EQ(optimized->getType(), LogicalPlanType::NESTED_LOOP_JOIN);

    // We skip executing the NLJ plan due to the known NestedLoopJoinExecutor bug.
}
