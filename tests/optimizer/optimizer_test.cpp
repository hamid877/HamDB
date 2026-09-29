#include "optimizer/optimizer.hpp"
#include "planner/logical_plan.hpp"
#include "catalog/schema.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace {

/// Build a trivial SeqScan plan with the given table name.
std::unique_ptr<hamdb::planner::SeqScanPlanNode>
makeSeqScan(const std::string& table)
{
    // Schema() default constructor produces an empty schema.
    return std::make_unique<hamdb::planner::SeqScanPlanNode>(
        hamdb::Schema{}, table, table);
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// IOptimizer interface contract tests (M9.1)
// ─────────────────────────────────────────────────────────────────────────────

/// HamDBOptimizer with no catalog must be constructible without crashing.
TEST(OptimizerTest, ConstructWithoutCatalog)
{
    hamdb::optimizer::HamDBOptimizer opt; // no catalog
    SUCCEED();
}

/// optimize(nullptr) must return nullptr safely.
TEST(OptimizerTest, OptimizeNullPlanReturnsNull)
{
    hamdb::optimizer::HamDBOptimizer opt;
    auto result = opt.optimize(nullptr);
    EXPECT_EQ(result, nullptr);
}

/// A single leaf SeqScan node passes through: result is non-null with correct type.
TEST(OptimizerTest, PassThroughSeqScanNode)
{
    hamdb::optimizer::HamDBOptimizer opt;
    auto result = opt.optimize(makeSeqScan("employees"));
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getType(), hamdb::planner::LogicalPlanType::SEQ_SCAN);
}

/// getAppliedRules() returns empty before any optimize() call.
TEST(OptimizerTest, AppliedRulesEmptyBeforeOptimize)
{
    hamdb::optimizer::HamDBOptimizer opt;
    EXPECT_TRUE(opt.getAppliedRules().empty());
}

/// clearAppliedRules() is safe to call when the trace is already empty.
TEST(OptimizerTest, ClearAppliedRulesIdempotent)
{
    hamdb::optimizer::HamDBOptimizer opt;
    opt.clearAppliedRules();
    EXPECT_TRUE(opt.getAppliedRules().empty());
}

/// After optimize() the applied-rule list is non-empty.
TEST(OptimizerTest, AppliedRulesNonEmptyAfterOptimize)
{
    hamdb::optimizer::HamDBOptimizer opt;
    [[maybe_unused]] auto result = opt.optimize(makeSeqScan("orders"));
    EXPECT_FALSE(opt.getAppliedRules().empty());
}

/// clearAppliedRules() resets the trace populated by a previous optimize().
TEST(OptimizerTest, ClearAppliedRulesAfterOptimize)
{
    hamdb::optimizer::HamDBOptimizer opt;
    [[maybe_unused]] auto r1 = opt.optimize(makeSeqScan("orders"));
    ASSERT_FALSE(opt.getAppliedRules().empty());

    opt.clearAppliedRules();
    EXPECT_TRUE(opt.getAppliedRules().empty());
}

/// Multiple sequential optimize() calls produce non-null results and don't
/// corrupt internal state.
TEST(OptimizerTest, RepeatedOptimizeCalls)
{
    hamdb::optimizer::HamDBOptimizer opt;
    for (int i = 0; i < 5; ++i) {
        opt.clearAppliedRules();
        auto result = opt.optimize(makeSeqScan("t" + std::to_string(i)));
        ASSERT_NE(result, nullptr) << "iteration " << i;
        EXPECT_FALSE(opt.getAppliedRules().empty()) << "iteration " << i;
    }
}

/// IOptimizer* polymorphic usage: callers depend only on the abstract interface.
TEST(OptimizerTest, UsableViaIOptimizerPointer)
{
    std::unique_ptr<hamdb::optimizer::IOptimizer> opt =
        std::make_unique<hamdb::optimizer::HamDBOptimizer>();

    auto result = opt->optimize(makeSeqScan("products"));
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(opt->getAppliedRules().empty());

    opt->clearAppliedRules();
    EXPECT_TRUE(opt->getAppliedRules().empty());
}

/// optimize() with a two-level FILTER → SEQ_SCAN plan must return non-null.
TEST(OptimizerTest, TwoLevelPlanPassThrough)
{
    hamdb::optimizer::HamDBOptimizer opt;

    auto scan = makeSeqScan("items");
    auto filter = std::make_unique<hamdb::planner::FilterPlanNode>(
        hamdb::Schema{}, nullptr);
    filter->addChild(std::move(scan));

    auto result = opt.optimize(std::move(filter));
    ASSERT_NE(result, nullptr);
    // Predicate pushdown may collapse FILTER → SEQ_SCAN; either type is valid.
    EXPECT_TRUE(
        result->getType() == hamdb::planner::LogicalPlanType::FILTER ||
        result->getType() == hamdb::planner::LogicalPlanType::SEQ_SCAN);
}

/// The applied-rule names returned are deterministic across two identical runs.
TEST(OptimizerTest, AppliedRulesDeterministic)
{
    hamdb::optimizer::HamDBOptimizer opt1;
    hamdb::optimizer::HamDBOptimizer opt2;

    [[maybe_unused]] auto r1 = opt1.optimize(makeSeqScan("tbl"));
    [[maybe_unused]] auto r2 = opt2.optimize(makeSeqScan("tbl"));

    EXPECT_EQ(opt1.getAppliedRules(), opt2.getAppliedRules());
}
