#pragma once

#include "catalog/statistics.hpp"
#include "planner/logical_plan.hpp"
#include <functional>
#include <string>

namespace hamdb::optimizer {

/**
 * @brief Statistics lookup callback type.
 *
 * Given a table name, returns a TableStatistics (row_count + column_stats).
 * Returns a zero-row TableStatistics when no statistics are available.
 */
using StatsLookup =
    std::function<TableStatistics(const std::string& table_name)>;

/**
 * @brief Extensible, recursive cost model for logical plan nodes.
 *
 * All costs are dimensionless I/O-equivalent units.  The formulas are:
 *
 *   SeqScan  ≈  row_count
 *   Filter   ≈  child_cost + child_rows
 *   NLJ      ≈  left_rows × right_rows + left_cost + right_cost
 *   HashJoin ≈  left_rows + right_rows + left_cost + right_cost
 *   Sort     ≈  N × log2(N) + child_cost          (N = child rows)
 *   TopK     ≈  N × log2(K) + child_cost          (K = limit bound)
 *
 * Cardinality estimation and cost estimation are intentionally kept in
 * separate classes: CardinalityEstimator owns selectivity maths; CostModel
 * owns the operator-cost formulas only.
 *
 * Limitations (by design for M9.8):
 *   - No join reordering.
 *   - No histogram-based or index-based costing.
 *   - No parallel or adaptive cost terms.
 */
class CostModel {
public:
    /**
     * @brief Construct with an optional statistics lookup.
     *
     * When @p stats is null (the default), all table cardinalities fall back
     * to kDefaultRows so cost comparisons still work in test contexts without
     * a live catalog.
     */
    explicit CostModel(StatsLookup stats = nullptr);

    /**
     * @brief Recursively estimate the cost of executing @p node.
     *
     * @param node  Root of the logical plan subtree.
     * @return      Estimated cost in dimensionless I/O-equivalent units.
     */
    [[nodiscard]] double estimateCost(
        const planner::LogicalPlanNode& node) const;

    /**
     * @brief Estimate the output row count of @p node.
     *
     * Used internally to pass cardinalities up the tree and by external
     * callers (e.g. CostBasedJoinRule) that need sibling-child cardinalities.
     */
    [[nodiscard]] double estimateRows(
        const planner::LogicalPlanNode& node) const;

private:
    StatsLookup stats_;

    /// Fallback cardinality when no statistics are available.
    static constexpr double kDefaultRows = 1000.0;

    /// Fallback limit bound for TopK when no constant is evaluable.
    static constexpr double kDefaultK = 10.0;

    [[nodiscard]] double rowsForScan(const std::string& table_name) const;
};

} // namespace hamdb::optimizer
