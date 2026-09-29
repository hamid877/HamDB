#pragma once
#include "planner/logical_plan.hpp"
#include "planner/physical_plan.hpp"
#include "executor/expression.hpp"
#include <memory>
#include <vector>
#include <utility>

namespace hamdb {
    enum class OrderByDirection {
        ASC,
        DESC
    };
}

namespace hamdb::planner {

class LogicalOrderByNode : public LogicalPlanNode {
public:
    LogicalOrderByNode(Schema output_schema, std::vector<std::pair<std::unique_ptr<hamdb::Expression>, bool>> order_by)
        : LogicalPlanNode(LogicalPlanType::ORDER_BY, std::move(output_schema)),
          order_by_(std::move(order_by)) {}
          
    const std::vector<std::pair<std::unique_ptr<hamdb::Expression>, bool>>& getOrderBy() const { return order_by_; }
    std::vector<std::pair<std::unique_ptr<hamdb::Expression>, bool>>& getMutableOrderBy() { return order_by_; }
private:
    std::vector<std::pair<std::unique_ptr<hamdb::Expression>, bool>> order_by_;
    friend class PhysicalPlanner;
};

class OrderByPlan : public AbstractPlanNode {
public:
    OrderByPlan(Schema output_schema, std::vector<std::pair<hamdb::OrderByDirection, std::unique_ptr<hamdb::Expression>>> order_bys)
        : AbstractPlanNode(PhysicalPlanType::ORDER_BY, std::move(output_schema)),
          order_bys_(std::move(order_bys)) {}
          
    const std::vector<std::pair<hamdb::OrderByDirection, std::unique_ptr<hamdb::Expression>>>& getOrderBy() const { return order_bys_; }
    std::vector<std::pair<hamdb::OrderByDirection, std::unique_ptr<hamdb::Expression>>>& getMutableOrderBy() { return order_bys_; }
private:
    std::vector<std::pair<hamdb::OrderByDirection, std::unique_ptr<hamdb::Expression>>> order_bys_;
};

} // namespace hamdb::planner
