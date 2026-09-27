#pragma once

#include "planner/logical_plan.hpp"
#include "planner/physical_plan.hpp"
#include "executor/execution_stats.hpp"
#include <string>
#include <memory>
#include <vector>

namespace hamdb::planner {

struct FormattedPlanNode {
    std::string name;
    std::string details;
    std::shared_ptr<executor::ExecutionStats> stats;
    std::vector<FormattedPlanNode> children;
};

class PlanFormatter {
public:
    static FormattedPlanNode buildFormattedTree(const LogicalPlanNode* plan);
    static FormattedPlanNode buildFormattedTree(const AbstractPlanNode* plan);
    static std::string renderTree(const FormattedPlanNode& node, bool analyze, const std::string& prefix = "", bool is_last = true);
    static void computeRowsIn(FormattedPlanNode& node);
};

} // namespace hamdb::planner
