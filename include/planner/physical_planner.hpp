#pragma once

#include "catalog/catalog_manager.hpp"
#include "planner/logical_plan.hpp"
#include "planner/physical_plan.hpp"
#include <memory>

namespace hamdb::planner {

class PhysicalPlanner {
public:
    PhysicalPlanner(CatalogManager* catalog) : catalog_(catalog) {}

    std::unique_ptr<AbstractPlanNode> plan(std::unique_ptr<LogicalPlanNode> logical_plan);
    
private:
    std::unique_ptr<AbstractPlanNode> planNode(std::unique_ptr<LogicalPlanNode> logical_node);

    CatalogManager* catalog_;
};

} // namespace hamdb::planner
