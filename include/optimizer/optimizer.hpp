#pragma once

#include "optimizer/rule_executor.hpp"
#include "planner/logical_plan.hpp"
#include <memory>
#include <string>
#include <vector>

// Forward declaration – avoids pulling the full catalog header into every
// translation unit that only needs the IOptimizer interface.
namespace hamdb {
class CatalogManager;
} // namespace hamdb

namespace hamdb::optimizer {

/**
 * @brief Abstract optimizer interface.
 *
 * Decouples callers from the concrete rule-application strategy.
 * Implementations receive a logical plan tree and return an (optionally
 * rewritten) logical plan tree that is semantically equivalent.
 *
 * For M9.1 the contract is intentionally minimal:
 *   - optimize() may return the input plan unchanged.
 *   - getAppliedRules() / clearAppliedRules() support EXPLAIN output.
 */
class IOptimizer {
public:
    virtual ~IOptimizer() = default;

    /**
     * @brief Optimize a logical plan.
     * @param plan  Ownership of the root logical plan node.
     * @return      Ownership of the (possibly rewritten) plan root.
     */
    [[nodiscard]] virtual std::unique_ptr<planner::LogicalPlanNode>
    optimize(std::unique_ptr<planner::LogicalPlanNode> plan) = 0;

    /**
     * @brief Names of rules that were applied during the last optimize() call.
     */
    [[nodiscard]] virtual const std::vector<std::string>&
    getAppliedRules() const = 0;

    /// Reset the applied-rule trace for the next optimize() call.
    virtual void clearAppliedRules() = 0;
};

/**
 * @brief Production optimizer built on top of RuleExecutor.
 *
 * Owns a RuleExecutor and registers the standard rule set during construction.
 * Callers interact only through the IOptimizer interface so the rule set can
 * change without affecting callsites.
 *
 * Pass @p catalog = nullptr to skip catalog-dependent rules (e.g. IndexScanRule)
 * when running in test contexts that have no live catalog.
 */
class HamDBOptimizer : public IOptimizer {
public:
    /**
     * @brief Construct with the standard rule set.
     * @param catalog  CatalogManager pointer; may be nullptr for rule-only tests.
     */
    explicit HamDBOptimizer(CatalogManager* catalog = nullptr);

    [[nodiscard]] std::unique_ptr<planner::LogicalPlanNode>
    optimize(std::unique_ptr<planner::LogicalPlanNode> plan) override;

    [[nodiscard]] const std::vector<std::string>& getAppliedRules() const override;

    void clearAppliedRules() override;

private:
    RuleExecutor rule_executor_;
};

} // namespace hamdb::optimizer
