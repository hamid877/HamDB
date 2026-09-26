#pragma once

#include "binder/bound_statement.hpp"
#include "catalog/catalog_manager.hpp"
#include "planner/logical_plan.hpp"
#include <memory>

namespace hamdb::planner {

class Planner {
public:
    explicit Planner(CatalogManager* catalog) : catalog_(catalog) {}

    std::unique_ptr<LogicalPlanNode> plan(std::unique_ptr<binder::BoundStatement> statement);

private:
    CatalogManager* catalog_;

    std::unique_ptr<LogicalPlanNode> planSelect(binder::BoundSelectStatement* stmt);
    std::unique_ptr<LogicalPlanNode> planInsert(binder::BoundInsertStatement* stmt);
    std::unique_ptr<LogicalPlanNode> planUpdate(binder::BoundUpdateStatement* stmt);
    std::unique_ptr<LogicalPlanNode> planDelete(binder::BoundDeleteStatement* stmt);
    static std::unique_ptr<LogicalPlanNode> planValues(binder::BoundValuesStatement* stmt);

    static ColumnType typeIdToColumnType(TypeId type_id);
};

} // namespace hamdb::planner
