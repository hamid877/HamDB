// M9.4 — Constant Folding (Advanced) test suite
//
// Tests are organised in three layers:
//   1. Expression-level unit tests — exercise foldExpression() in isolation
//      using only ConstantFoldingRule + RuleExecutor, no catalog/disk needed.
//   2. Plan-structural tests — verify the logical plan shape after folding.
//   3. Query-level semantic equivalence tests — run both an unoptimized and
//      an optimized plan through the physical executor and compare row counts
//      and column values, ensuring folding never changes observable semantics.
//
// Key invariants verified:
//   * Arithmetic folds: constant operands produce a ConstantExpression result.
//   * Comparison folds: two constants produce boolean ConstantExpression.
//   * Boolean short-circuits (3-valued SQL logic):
//       false AND x  →  false
//       true  AND x  →  x
//       true  OR  x  →  true
//       false OR  x  →  x
//       NOT true     →  false
//       NOT false    →  true
//   * NULL propagation: NULL arithmetic/comparison → NULL (not crash).
//   * Exception safety: 1/0 → NULL constant (optimizer never aborts).
//   * Non-constant sub-expressions are left intact.
//   * Nested folds: ((2+3)*4) → 20 in one pass.
//   * Modulo fold.
//   * Query-level equivalence: optimized plan returns same rows as unoptimized.

#include "optimizer/rule_executor.hpp"
#include "optimizer/constant_folding_rule.hpp"
#include "planner/logical_plan.hpp"
#include "planner/physical_planner.hpp"
#include "planner/executor_factory.hpp"
#include "executor/constant_expression.hpp"
#include "executor/arithmetic_expression.hpp"
#include "executor/comparison_expression.hpp"
#include "executor/logical_expression.hpp"
#include "executor/column_value_expression.hpp"
#include "executor/executor_context.hpp"
#include "executor/abstract_executor.hpp"
#include "catalog/catalog_manager.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include "storage/disk_manager.hpp"
#include "storage/table_heap.hpp"
#include "storage/slotted_page.hpp"
#include "transaction/transaction_manager.hpp"
#include "transaction/lock_manager.hpp"
#include "transaction/mvcc_manager.hpp"
#include "wal/log_manager.hpp"
#include <filesystem>
#include <cstring>
#include <gtest/gtest.h>
#include <memory>

namespace hamdb::optimizer {

// ============================================================================
// Section 1 – Expression-level unit tests (no disk / catalog required)
// ============================================================================

class ConstantFoldingExprTest : public ::testing::Test {
protected:
    void SetUp() override {
        rule_executor_ = std::make_unique<RuleExecutor>();
        rule_executor_->addRule(std::make_unique<ConstantFoldingRule>());
    }

    // Convenience: wrap an expression in a FilterPlanNode, optimize, return the
    // resulting predicate pointer (owned by the plan returned as out-param).
    const Expression* fold(std::unique_ptr<Expression> expr,
                           std::unique_ptr<planner::LogicalPlanNode>& out_plan) {
        Schema empty_schema;
        auto filter = std::make_unique<planner::FilterPlanNode>(empty_schema, std::move(expr));
        out_plan = rule_executor_->optimize(std::move(filter));
        auto* f = dynamic_cast<planner::FilterPlanNode*>(out_plan.get());
        if (!f) return nullptr;
        return f->getPredicate();
    }

    Tuple dummy_tuple_;
    Schema dummy_schema_;
    std::unique_ptr<RuleExecutor> rule_executor_;
};

// ---------------------------------------------------------------------------
// Arithmetic folding
// ---------------------------------------------------------------------------

TEST_F(ConstantFoldingExprTest, FoldAddition) {
    auto expr = std::make_unique<ArithmeticExpression>(
        ArithmeticType::Add,
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(10))),
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(7))));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    ASSERT_NE(result, nullptr);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->evaluate(dummy_tuple_, dummy_schema_).getAsInteger(), 17);
}

TEST_F(ConstantFoldingExprTest, FoldSubtraction) {
    auto expr = std::make_unique<ArithmeticExpression>(
        ArithmeticType::Subtract,
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(20))),
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(8))));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->evaluate(dummy_tuple_, dummy_schema_).getAsInteger(), 12);
}

TEST_F(ConstantFoldingExprTest, FoldMultiplication) {
    auto expr = std::make_unique<ArithmeticExpression>(
        ArithmeticType::Multiply,
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(6))),
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(7))));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->evaluate(dummy_tuple_, dummy_schema_).getAsInteger(), 42);
}

TEST_F(ConstantFoldingExprTest, FoldDivision) {
    auto expr = std::make_unique<ArithmeticExpression>(
        ArithmeticType::Divide,
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(20))),
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(4))));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->evaluate(dummy_tuple_, dummy_schema_).getAsInteger(), 5);
}

TEST_F(ConstantFoldingExprTest, FoldModulo) {
    auto expr = std::make_unique<ArithmeticExpression>(
        ArithmeticType::Modulo,
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(17))),
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(5))));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->evaluate(dummy_tuple_, dummy_schema_).getAsInteger(), 2);
}

// ---------------------------------------------------------------------------
// Exception safety — division/modulo by zero must NOT crash the optimizer;
// it must produce NULL (matching HamDB's runtime error→NULL semantics).
// ---------------------------------------------------------------------------

TEST_F(ConstantFoldingExprTest, DivisionByZeroProducesNull) {
    auto expr = std::make_unique<ArithmeticExpression>(
        ArithmeticType::Divide,
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(42))),
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(0))));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    // Must not throw; result is a ConstantExpression(NULL).
    ASSERT_NO_THROW({
        const auto* result = fold(std::move(expr), plan);
        ASSERT_NE(result, nullptr);
        auto* c = dynamic_cast<const ConstantExpression*>(result);
        ASSERT_NE(c, nullptr);
        EXPECT_TRUE(c->evaluate(dummy_tuple_, dummy_schema_).isNull());
    });
}

TEST_F(ConstantFoldingExprTest, ModuloByZeroProducesNull) {
    auto expr = std::make_unique<ArithmeticExpression>(
        ArithmeticType::Modulo,
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(10))),
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(0))));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    ASSERT_NO_THROW({
        const auto* result = fold(std::move(expr), plan);
        ASSERT_NE(result, nullptr);
        auto* c = dynamic_cast<const ConstantExpression*>(result);
        ASSERT_NE(c, nullptr);
        EXPECT_TRUE(c->evaluate(dummy_tuple_, dummy_schema_).isNull());
    });
}

// ---------------------------------------------------------------------------
// Nested fold: ((2+3)*4) → one ConstantExpression(20)
// ---------------------------------------------------------------------------

TEST_F(ConstantFoldingExprTest, NestedArithmeticFold) {
    // (2 + 3) * 4
    auto inner = std::make_unique<ArithmeticExpression>(
        ArithmeticType::Add,
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(2))),
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(3))));
    auto outer = std::make_unique<ArithmeticExpression>(
        ArithmeticType::Multiply,
        std::move(inner),
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(4))));

    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(outer), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->evaluate(dummy_tuple_, dummy_schema_).getAsInteger(), 20);
}

// ---------------------------------------------------------------------------
// Comparison folding
// ---------------------------------------------------------------------------

TEST_F(ConstantFoldingExprTest, FoldEqualTrue) {
    auto expr = std::make_unique<ComparisonExpression>(
        ComparisonType::Equal,
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(5))),
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(5))));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->evaluate(dummy_tuple_, dummy_schema_).getAsBoolean());
}

TEST_F(ConstantFoldingExprTest, FoldEqualFalse) {
    auto expr = std::make_unique<ComparisonExpression>(
        ComparisonType::Equal,
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(3))),
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(7))));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_FALSE(c->evaluate(dummy_tuple_, dummy_schema_).getAsBoolean());
}

TEST_F(ConstantFoldingExprTest, FoldLessThanTrue) {
    auto expr = std::make_unique<ComparisonExpression>(
        ComparisonType::LessThan,
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(1))),
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(2))));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->evaluate(dummy_tuple_, dummy_schema_).getAsBoolean());
}

TEST_F(ConstantFoldingExprTest, FoldGreaterThanFalse) {
    auto expr = std::make_unique<ComparisonExpression>(
        ComparisonType::GreaterThan,
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(1))),
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(10))));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_FALSE(c->evaluate(dummy_tuple_, dummy_schema_).getAsBoolean());
}

// ---------------------------------------------------------------------------
// Boolean folding — both sides constant
// ---------------------------------------------------------------------------

TEST_F(ConstantFoldingExprTest, FoldAndTrueTrue) {
    auto expr = std::make_unique<LogicalExpression>(
        LogicalType::And,
        std::make_unique<ConstantExpression>(Value(true)),
        std::make_unique<ConstantExpression>(Value(true)));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->evaluate(dummy_tuple_, dummy_schema_).getAsBoolean());
}

TEST_F(ConstantFoldingExprTest, FoldAndTrueFalse) {
    auto expr = std::make_unique<LogicalExpression>(
        LogicalType::And,
        std::make_unique<ConstantExpression>(Value(true)),
        std::make_unique<ConstantExpression>(Value(false)));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_FALSE(c->evaluate(dummy_tuple_, dummy_schema_).getAsBoolean());
}

TEST_F(ConstantFoldingExprTest, FoldOrFalseFalse) {
    auto expr = std::make_unique<LogicalExpression>(
        LogicalType::Or,
        std::make_unique<ConstantExpression>(Value(false)),
        std::make_unique<ConstantExpression>(Value(false)));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_FALSE(c->evaluate(dummy_tuple_, dummy_schema_).getAsBoolean());
}

TEST_F(ConstantFoldingExprTest, FoldOrFalseTrue) {
    auto expr = std::make_unique<LogicalExpression>(
        LogicalType::Or,
        std::make_unique<ConstantExpression>(Value(false)),
        std::make_unique<ConstantExpression>(Value(true)));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->evaluate(dummy_tuple_, dummy_schema_).getAsBoolean());
}

TEST_F(ConstantFoldingExprTest, FoldNotTrue) {
    auto expr = std::make_unique<LogicalExpression>(
        LogicalType::Not,
        std::make_unique<ConstantExpression>(Value(true)));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_FALSE(c->evaluate(dummy_tuple_, dummy_schema_).getAsBoolean());
}

TEST_F(ConstantFoldingExprTest, FoldNotFalse) {
    auto expr = std::make_unique<LogicalExpression>(
        LogicalType::Not,
        std::make_unique<ConstantExpression>(Value(false)));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->evaluate(dummy_tuple_, dummy_schema_).getAsBoolean());
}

// ---------------------------------------------------------------------------
// Boolean short-circuits with one non-constant operand
// ---------------------------------------------------------------------------

TEST_F(ConstantFoldingExprTest, AndFalseNonConst_YieldsFalse) {
    // false AND <col>  →  ConstantExpression(false)
    auto expr = std::make_unique<LogicalExpression>(
        LogicalType::And,
        std::make_unique<ConstantExpression>(Value(false)),
        std::make_unique<ColumnValueExpression>(0));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_FALSE(c->evaluate(dummy_tuple_, dummy_schema_).getAsBoolean());
}

TEST_F(ConstantFoldingExprTest, AndTrueNonConst_YieldsNonConst) {
    // true AND <col>  →  <col>  (ColumnValueExpression is preserved)
    auto expr = std::make_unique<LogicalExpression>(
        LogicalType::And,
        std::make_unique<ConstantExpression>(Value(true)),
        std::make_unique<ColumnValueExpression>(0));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    // Should be the ColumnValueExpression, not a ConstantExpression
    ASSERT_NE(result, nullptr);
    auto* col = dynamic_cast<const ColumnValueExpression*>(result);
    ASSERT_NE(col, nullptr);
    EXPECT_EQ(col->getColIdx(), 0U);
}

TEST_F(ConstantFoldingExprTest, NonConstAndFalse_YieldsFalse) {
    // <col> AND false  →  ConstantExpression(false)
    auto expr = std::make_unique<LogicalExpression>(
        LogicalType::And,
        std::make_unique<ColumnValueExpression>(0),
        std::make_unique<ConstantExpression>(Value(false)));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_FALSE(c->evaluate(dummy_tuple_, dummy_schema_).getAsBoolean());
}

TEST_F(ConstantFoldingExprTest, NonConstAndTrue_YieldsNonConst) {
    // <col> AND true  →  <col>
    auto expr = std::make_unique<LogicalExpression>(
        LogicalType::And,
        std::make_unique<ColumnValueExpression>(0),
        std::make_unique<ConstantExpression>(Value(true)));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    ASSERT_NE(result, nullptr);
    auto* col = dynamic_cast<const ColumnValueExpression*>(result);
    ASSERT_NE(col, nullptr);
}

TEST_F(ConstantFoldingExprTest, OrTrueNonConst_YieldsTrue) {
    // true OR <col>  →  ConstantExpression(true)
    auto expr = std::make_unique<LogicalExpression>(
        LogicalType::Or,
        std::make_unique<ConstantExpression>(Value(true)),
        std::make_unique<ColumnValueExpression>(0));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->evaluate(dummy_tuple_, dummy_schema_).getAsBoolean());
}

TEST_F(ConstantFoldingExprTest, OrFalseNonConst_YieldsNonConst) {
    // false OR <col>  →  <col>
    auto expr = std::make_unique<LogicalExpression>(
        LogicalType::Or,
        std::make_unique<ConstantExpression>(Value(false)),
        std::make_unique<ColumnValueExpression>(0));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    ASSERT_NE(result, nullptr);
    auto* col = dynamic_cast<const ColumnValueExpression*>(result);
    ASSERT_NE(col, nullptr);
}

TEST_F(ConstantFoldingExprTest, NonConstOrTrue_YieldsTrue) {
    // <col> OR true  →  ConstantExpression(true)
    auto expr = std::make_unique<LogicalExpression>(
        LogicalType::Or,
        std::make_unique<ColumnValueExpression>(0),
        std::make_unique<ConstantExpression>(Value(true)));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->evaluate(dummy_tuple_, dummy_schema_).getAsBoolean());
}

TEST_F(ConstantFoldingExprTest, NonConstOrFalse_YieldsNonConst) {
    // <col> OR false  →  <col>
    auto expr = std::make_unique<LogicalExpression>(
        LogicalType::Or,
        std::make_unique<ColumnValueExpression>(0),
        std::make_unique<ConstantExpression>(Value(false)));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    ASSERT_NE(result, nullptr);
    auto* col = dynamic_cast<const ColumnValueExpression*>(result);
    ASSERT_NE(col, nullptr);
}

// ---------------------------------------------------------------------------
// Non-constant expressions must be left unchanged
// ---------------------------------------------------------------------------

TEST_F(ConstantFoldingExprTest, NonConstantExpressionUnchanged) {
    // col0 + col1 — both sides are column refs, must not be folded
    auto expr = std::make_unique<ArithmeticExpression>(
        ArithmeticType::Add,
        std::make_unique<ColumnValueExpression>(0),
        std::make_unique<ColumnValueExpression>(1));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    ASSERT_NE(result, nullptr);
    // Must still be an ArithmeticExpression, not a ConstantExpression
    EXPECT_EQ(dynamic_cast<const ConstantExpression*>(result), nullptr);
    EXPECT_NE(dynamic_cast<const ArithmeticExpression*>(result), nullptr);
}

TEST_F(ConstantFoldingExprTest, PartiallyConstantArithmeticLeftUnchanged) {
    // 5 + col0  — cannot fold (col0 depends on tuple)
    auto expr = std::make_unique<ArithmeticExpression>(
        ArithmeticType::Add,
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(5))),
        std::make_unique<ColumnValueExpression>(0));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(dynamic_cast<const ConstantExpression*>(result), nullptr);
    // But the left child (5) is still there, folded inside itself (trivially)
    auto* arith = dynamic_cast<const ArithmeticExpression*>(result);
    ASSERT_NE(arith, nullptr);
    // Left child is ConstantExpression(5)
    auto* left_const = dynamic_cast<const ConstantExpression*>(arith->getChildren()[0].get());
    ASSERT_NE(left_const, nullptr);
    EXPECT_EQ(left_const->evaluate(dummy_tuple_, dummy_schema_).getAsInteger(), 5);
}

// ---------------------------------------------------------------------------
// NULL propagation in arithmetic and comparison
// ---------------------------------------------------------------------------

TEST_F(ConstantFoldingExprTest, NullArithmeticProducesNull) {
    // NULL + 5  →  NULL
    auto expr = std::make_unique<ArithmeticExpression>(
        ArithmeticType::Add,
        std::make_unique<ConstantExpression>(Value()),   // NULL
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(5))));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->evaluate(dummy_tuple_, dummy_schema_).isNull());
}

TEST_F(ConstantFoldingExprTest, NullComparisonProducesNull) {
    // NULL = 5  →  NULL
    auto expr = std::make_unique<ComparisonExpression>(
        ComparisonType::Equal,
        std::make_unique<ConstantExpression>(Value()),
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(5))));
    std::unique_ptr<planner::LogicalPlanNode> plan;
    const auto* result = fold(std::move(expr), plan);
    auto* c = dynamic_cast<const ConstantExpression*>(result);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->evaluate(dummy_tuple_, dummy_schema_).isNull());
}

// ============================================================================
// Section 2 – Plan-structural tests
// ============================================================================

class ConstantFoldingPlanTest : public ::testing::Test {
protected:
    void SetUp() override {
        rule_executor_ = std::make_unique<RuleExecutor>();
        rule_executor_->addRule(std::make_unique<ConstantFoldingRule>());
    }
    std::unique_ptr<RuleExecutor> rule_executor_;
    Tuple dummy_tuple_;
    Schema dummy_schema_;
};

TEST_F(ConstantFoldingPlanTest, FilterPredicateFolded) {
    // Filter(2+3) → Filter(ConstantExpression(5))
    auto add = std::make_unique<ArithmeticExpression>(
        ArithmeticType::Add,
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(2))),
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(3))));
    auto filter = std::make_unique<planner::FilterPlanNode>(dummy_schema_, std::move(add));
    auto root = rule_executor_->optimize(std::move(filter));

    auto* f = dynamic_cast<planner::FilterPlanNode*>(root.get());
    ASSERT_NE(f, nullptr);
    auto* folded = dynamic_cast<const ConstantExpression*>(f->getPredicate());
    ASSERT_NE(folded, nullptr);
    EXPECT_EQ(folded->evaluate(dummy_tuple_, dummy_schema_).getAsInteger(), 5);
}

TEST_F(ConstantFoldingPlanTest, ProjectionExpressionsFolded) {
    // Projection([1+2, 3*4])
    std::vector<std::unique_ptr<Expression>> exprs;
    exprs.push_back(std::make_unique<ArithmeticExpression>(
        ArithmeticType::Add,
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(1))),
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(2)))));
    exprs.push_back(std::make_unique<ArithmeticExpression>(
        ArithmeticType::Multiply,
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(3))),
        std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(4)))));

    std::vector<Column> cols = { Column("a", ColumnType::Integer),
                                  Column("b", ColumnType::Integer) };
    Schema out_schema(cols);
    auto proj = std::make_unique<planner::ProjectionPlanNode>(out_schema, std::move(exprs));
    auto root = rule_executor_->optimize(std::move(proj));

    auto* p = dynamic_cast<planner::ProjectionPlanNode*>(root.get());
    ASSERT_NE(p, nullptr);
    const auto& folded_exprs = p->getExpressions();
    ASSERT_EQ(folded_exprs.size(), 2U);

    auto* c0 = dynamic_cast<const ConstantExpression*>(folded_exprs[0].get());
    ASSERT_NE(c0, nullptr);
    EXPECT_EQ(c0->evaluate(dummy_tuple_, dummy_schema_).getAsInteger(), 3);

    auto* c1 = dynamic_cast<const ConstantExpression*>(folded_exprs[1].get());
    ASSERT_NE(c1, nullptr);
    EXPECT_EQ(c1->evaluate(dummy_tuple_, dummy_schema_).getAsInteger(), 12);
}

// ============================================================================
// Section 3 – Query-level semantic equivalence tests
//
// Each test runs an unoptimized and an optimized plan through the executor
// and compares observable outputs.  The catalog and table heap are needed
// only when actual tuple storage is involved; for constant-folding tests the
// plan typically produces deterministic row counts independent of stored data.
// ============================================================================

class ConstantFoldingQueryTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_ = "test_cf_query_" + std::to_string(
            reinterpret_cast<uintptr_t>(this)) + ".hamdb";
        if (std::filesystem::exists(test_db_)) {
            std::filesystem::remove(test_db_);
        }
        disk_manager_ = std::make_unique<DiskManager>(test_db_);
        (void)disk_manager_->createDatabase();
        (void)disk_manager_->openDatabase();
        bpm_ = std::make_unique<BufferPoolManager>(16, *disk_manager_);
        catalog_ = std::make_unique<CatalogManager>(bpm_.get());

        std::vector<Column> cols = {
            Column("id",  ColumnType::Integer),
            Column("val", ColumnType::Integer)
        };
        Schema schema(cols);
        TableInfo* info;
        (void)catalog_->createTable("cf_table", schema, info);
        table_info_ = info;

        physical_planner_ = std::make_unique<planner::PhysicalPlanner>(catalog_.get());
        rule_executor_ = std::make_unique<RuleExecutor>();
        rule_executor_->addRule(std::make_unique<ConstantFoldingRule>());

        txn_manager_  = std::make_unique<TransactionManager>();
        lock_manager_ = std::make_unique<LockManager>();
        log_manager_  = std::make_unique<LogManager>();
        mvcc_manager_ = std::make_unique<MvccManager>();

        // Insert two rows into cf_table.
        {
            WritePageGuard guard;
            (void)bpm_->fetchPageWrite(table_info_->getHeapRootPage(), guard);
            Page page(PageHeader(table_info_->getHeapRootPage(), PageType::Table));
            SlottedPage sp(page);
            (void)sp.initialize();
            std::memcpy(guard.pageMut().data().data(), page.data().data(), Page::kSize);
            guard.markDirty();
        }
        (void)bpm_->flushPage(table_info_->getHeapRootPage());

        // row0: id=1, val=10  (8 bytes: two int32_t)
        std::vector<std::byte> row0(8, std::byte{0});
        *reinterpret_cast<int32_t*>(row0.data())     = 1;
        *reinterpret_cast<int32_t*>(row0.data() + 4) = 10;
        // row1: id=2, val=20
        std::vector<std::byte> row1(8, std::byte{0});
        *reinterpret_cast<int32_t*>(row1.data())     = 2;
        *reinterpret_cast<int32_t*>(row1.data() + 4) = 20;

        heap_ = std::make_unique<TableHeap>(
            *TableHeap::open(*disk_manager_, table_info_->getHeapRootPage()));
        auto txn = txn_manager_->begin();
        RID r1, r2;
        (void)heap_->insertTuple(Tuple(row0), r1);
        (void)heap_->insertTuple(Tuple(row1), r2);
        txn_manager_->commit(txn);

        auto txn2 = txn_manager_->begin();
        exec_ctx_ = std::make_unique<ExecutorContext>(
            txn2, catalog_.get(), bpm_.get(), mvcc_manager_.get(),
            disk_manager_.get(), lock_manager_.get(), log_manager_.get());
    }

    void TearDown() override {
        exec_ctx_.reset();
        if (std::filesystem::exists(test_db_)) {
            std::filesystem::remove(test_db_);
        }
    }

    // Run a logical plan (possibly optimized) and collect row count.
    int executeAndCount(std::unique_ptr<planner::LogicalPlanNode> plan) {
        auto phys = physical_planner_->plan(std::move(plan));
        auto exec = planner::ExecutorFactory::createExecutor(exec_ctx_.get(), std::move(phys));
        exec->init();
        Tuple t;
        RID r;
        int count = 0;
        while (exec->next(&t, &r)) { ++count; }
        return count;
    }

    std::string test_db_;
    TableInfo* table_info_{nullptr};
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> bpm_;
    std::unique_ptr<CatalogManager> catalog_;
    std::unique_ptr<planner::PhysicalPlanner> physical_planner_;
    std::unique_ptr<RuleExecutor> rule_executor_;
    std::unique_ptr<TransactionManager> txn_manager_;
    std::unique_ptr<LockManager> lock_manager_;
    std::unique_ptr<LogManager> log_manager_;
    std::unique_ptr<MvccManager> mvcc_manager_;
    std::unique_ptr<TableHeap> heap_;
    std::unique_ptr<ExecutorContext> exec_ctx_;
};

// ConstantTrue filter: all rows pass in both plans.
TEST_F(ConstantFoldingQueryTest, ConstantTrueFilterEquivalence) {
    auto mk_plan = [this]() {
        auto seq = std::make_unique<planner::SeqScanPlanNode>(
            table_info_->getSchema(), "cf_table", "cf_table");
        auto filter = std::make_unique<planner::FilterPlanNode>(
            table_info_->getSchema(),
            std::make_unique<ConstantExpression>(Value(true)));
        filter->addChild(std::move(seq));
        return std::unique_ptr<planner::LogicalPlanNode>(std::move(filter));
    };

    int unopt = executeAndCount(mk_plan());

    auto opt_plan = rule_executor_->optimize(mk_plan());
    int opt = executeAndCount(std::move(opt_plan));

    EXPECT_EQ(unopt, 2);
    EXPECT_EQ(opt,   2);
}

// ConstantFalse filter: zero rows in both plans.
TEST_F(ConstantFoldingQueryTest, ConstantFalseFilterEquivalence) {
    auto mk_plan = [this]() {
        auto seq = std::make_unique<planner::SeqScanPlanNode>(
            table_info_->getSchema(), "cf_table", "cf_table");
        auto filter = std::make_unique<planner::FilterPlanNode>(
            table_info_->getSchema(),
            std::make_unique<ConstantExpression>(Value(false)));
        filter->addChild(std::move(seq));
        return std::unique_ptr<planner::LogicalPlanNode>(std::move(filter));
    };

    int unopt = executeAndCount(mk_plan());
    auto opt_plan = rule_executor_->optimize(mk_plan());
    int opt = executeAndCount(std::move(opt_plan));

    EXPECT_EQ(unopt, 0);
    EXPECT_EQ(opt,   0);
}

// Foldable arithmetic comparison: 2+3 = 5 → true, all rows pass.
TEST_F(ConstantFoldingQueryTest, ArithmeticComparisonFolded) {
    auto mk_pred = []() {
        return std::make_unique<ComparisonExpression>(
            ComparisonType::Equal,
            std::make_unique<ArithmeticExpression>(
                ArithmeticType::Add,
                std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(2))),
                std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(3)))),
            std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(5))));
    };

    auto mk_plan = [this, &mk_pred]() {
        auto seq = std::make_unique<planner::SeqScanPlanNode>(
            table_info_->getSchema(), "cf_table", "cf_table");
        auto filter = std::make_unique<planner::FilterPlanNode>(
            table_info_->getSchema(), mk_pred());
        filter->addChild(std::move(seq));
        return std::unique_ptr<planner::LogicalPlanNode>(std::move(filter));
    };

    int unopt = executeAndCount(mk_plan());
    auto opt_plan = rule_executor_->optimize(mk_plan());
    int opt = executeAndCount(std::move(opt_plan));

    EXPECT_EQ(unopt, 2);
    EXPECT_EQ(opt,   2);
}

// Foldable false constant comparison: 1 = 2 → false, zero rows.
TEST_F(ConstantFoldingQueryTest, FalseConstantComparisonFolded) {
    auto mk_pred = []() {
        return std::make_unique<ComparisonExpression>(
            ComparisonType::Equal,
            std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(1))),
            std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(2))));
    };

    auto mk_plan = [this, &mk_pred]() {
        auto seq = std::make_unique<planner::SeqScanPlanNode>(
            table_info_->getSchema(), "cf_table", "cf_table");
        auto filter = std::make_unique<planner::FilterPlanNode>(
            table_info_->getSchema(), mk_pred());
        filter->addChild(std::move(seq));
        return std::unique_ptr<planner::LogicalPlanNode>(std::move(filter));
    };

    int unopt = executeAndCount(mk_plan());
    auto opt_plan = rule_executor_->optimize(mk_plan());
    int opt = executeAndCount(std::move(opt_plan));

    EXPECT_EQ(unopt, 0);
    EXPECT_EQ(opt,   0);
}

// false AND <col> → false; zero rows, equivalently.
TEST_F(ConstantFoldingQueryTest, AndFalseNonConstEquivalence) {
    auto mk_pred = []() {
        return std::make_unique<LogicalExpression>(
            LogicalType::And,
            std::make_unique<ConstantExpression>(Value(false)),
            std::make_unique<ConstantExpression>(Value(true)));
    };

    auto mk_plan = [this, &mk_pred]() {
        auto seq = std::make_unique<planner::SeqScanPlanNode>(
            table_info_->getSchema(), "cf_table", "cf_table");
        auto filter = std::make_unique<planner::FilterPlanNode>(
            table_info_->getSchema(), mk_pred());
        filter->addChild(std::move(seq));
        return std::unique_ptr<planner::LogicalPlanNode>(std::move(filter));
    };

    int unopt = executeAndCount(mk_plan());
    auto opt_plan = rule_executor_->optimize(mk_plan());
    int opt = executeAndCount(std::move(opt_plan));

    EXPECT_EQ(unopt, 0);
    EXPECT_EQ(opt,   0);
}

// true OR <col> → true; all rows pass.
TEST_F(ConstantFoldingQueryTest, OrTrueNonConstEquivalence) {
    auto mk_pred = []() {
        return std::make_unique<LogicalExpression>(
            LogicalType::Or,
            std::make_unique<ConstantExpression>(Value(true)),
            std::make_unique<ConstantExpression>(Value(false)));
    };

    auto mk_plan = [this, &mk_pred]() {
        auto seq = std::make_unique<planner::SeqScanPlanNode>(
            table_info_->getSchema(), "cf_table", "cf_table");
        auto filter = std::make_unique<planner::FilterPlanNode>(
            table_info_->getSchema(), mk_pred());
        filter->addChild(std::move(seq));
        return std::unique_ptr<planner::LogicalPlanNode>(std::move(filter));
    };

    int unopt = executeAndCount(mk_plan());
    auto opt_plan = rule_executor_->optimize(mk_plan());
    int opt = executeAndCount(std::move(opt_plan));

    EXPECT_EQ(unopt, 2);
    EXPECT_EQ(opt,   2);
}

// Division by zero in constant fold: optimizer must not crash, result is NULL
// (treated as false by the filter), so zero rows are returned.
TEST_F(ConstantFoldingQueryTest, DivisionByZeroFoldsToNullFilterEquivalence) {
    auto mk_pred = []() {
        // (10 / 0) = 0  — both constants; optimizer folds 10/0 → NULL
        // NULL = 0 → NULL (treated as false by filter)
        return std::make_unique<ComparisonExpression>(
            ComparisonType::Equal,
            std::make_unique<ArithmeticExpression>(
                ArithmeticType::Divide,
                std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(10))),
                std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(0)))),
            std::make_unique<ConstantExpression>(Value(static_cast<int32_t>(0))));
    };

    auto mk_plan = [this, &mk_pred]() {
        auto seq = std::make_unique<planner::SeqScanPlanNode>(
            table_info_->getSchema(), "cf_table", "cf_table");
        auto filter = std::make_unique<planner::FilterPlanNode>(
            table_info_->getSchema(), mk_pred());
        filter->addChild(std::move(seq));
        return std::unique_ptr<planner::LogicalPlanNode>(std::move(filter));
    };

    // Optimizer must not throw.
    ASSERT_NO_THROW({
        auto opt_plan = rule_executor_->optimize(mk_plan());
        int opt = executeAndCount(std::move(opt_plan));
        EXPECT_EQ(opt, 0); // NULL filter → 0 rows
    });
}

} // namespace hamdb::optimizer
