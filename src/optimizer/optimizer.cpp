#include "optimizer/optimizer.hpp"
#include "optimizer/predicate_pushdown_rule.hpp"
#include "optimizer/projection_pruning_rule.hpp"
#include "optimizer/constant_folding_rule.hpp"
#include "optimizer/index_scan_rule.hpp"
#include "optimizer/sort_limit_rule.hpp"
#include <memory>

namespace hamdb::optimizer {

HamDBOptimizer::HamDBOptimizer(CatalogManager* catalog)
{
    // Register rules in the order they should be applied (bottom-up traversal
    // is handled inside RuleExecutor).  The set is intentionally the same as
    // the one previously hard-coded in Shell::initDB() so behavior is
    // unchanged for existing callers.
    rule_executor_.addRule(std::make_unique<PredicatePushdownRule>());
    rule_executor_.addRule(std::make_unique<ProjectionPruningRule>());
    rule_executor_.addRule(std::make_unique<ConstantFoldingRule>());
    // IndexScanRule requires a live catalog; skip it when catalog is null so
    // unit tests that don't need catalog access can instantiate HamDBOptimizer.
    if (catalog != nullptr) {
        rule_executor_.addRule(std::make_unique<IndexScanRule>(catalog));
    }
    rule_executor_.addRule(std::make_unique<SortLimitRule>());
}

std::unique_ptr<planner::LogicalPlanNode>
HamDBOptimizer::optimize(std::unique_ptr<planner::LogicalPlanNode> plan)
{
    return rule_executor_.optimize(std::move(plan));
}

const std::vector<std::string>& HamDBOptimizer::getAppliedRules() const
{
    return rule_executor_.getAppliedRules();
}

void HamDBOptimizer::clearAppliedRules()
{
    rule_executor_.clearAppliedRules();
}

} // namespace hamdb::optimizer
