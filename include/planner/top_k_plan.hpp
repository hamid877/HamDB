#pragma once

#include "planner/logical_plan.hpp"
#include "planner/physical_plan.hpp"
#include "planner/order_by_plan.hpp"
#include "executor/expression.hpp"
#include <memory>
#include <vector>
#include <utility>

namespace hamdb::planner {

class LogicalTopKNode : public LogicalPlanNode {
public:
    LogicalTopKNode(Schema output_schema, std::vector<std::pair<std::unique_ptr<hamdb::Expression>, bool>> order_by, std::unique_ptr<hamdb::Expression> limit, std::unique_ptr<hamdb::Expression> offset)
        : LogicalPlanNode(LogicalPlanType::TOP_K, std::move(output_schema)),
          order_by_(std::move(order_by)), limit_(std::move(limit)), offset_(std::move(offset)) {}

    const std::vector<std::pair<std::unique_ptr<hamdb::Expression>, bool>>& getOrderBy() const { return order_by_; }
    std::vector<std::pair<std::unique_ptr<hamdb::Expression>, bool>>& getMutableOrderBy() { return order_by_; }

    const hamdb::Expression* getLimit() const { return limit_.get(); }
    const hamdb::Expression* getOffset() const { return offset_.get(); }
    std::unique_ptr<hamdb::Expression> takeLimit() { return std::move(limit_); }
    std::unique_ptr<hamdb::Expression> takeOffset() { return std::move(offset_); }
    void setLimit(std::unique_ptr<hamdb::Expression> limit) { limit_ = std::move(limit); }
    void setOffset(std::unique_ptr<hamdb::Expression> offset) { offset_ = std::move(offset); }

private:
    std::vector<std::pair<std::unique_ptr<hamdb::Expression>, bool>> order_by_;
    std::unique_ptr<hamdb::Expression> limit_;
    std::unique_ptr<hamdb::Expression> offset_;
    friend class PhysicalPlanner;
};

class TopKPlan : public AbstractPlanNode {
public:
    TopKPlan(Schema output_schema, std::vector<std::pair<hamdb::OrderByDirection, std::unique_ptr<hamdb::Expression>>> order_bys, std::size_t limit, std::size_t offset)
        : AbstractPlanNode(PhysicalPlanType::TOP_K, std::move(output_schema)),
          order_bys_(std::move(order_bys)), limit_(limit), offset_(offset) {}

    const std::vector<std::pair<hamdb::OrderByDirection, std::unique_ptr<hamdb::Expression>>>& getOrderBy() const { return order_bys_; }
    std::vector<std::pair<hamdb::OrderByDirection, std::unique_ptr<hamdb::Expression>>>& getMutableOrderBy() { return order_bys_; }
    std::size_t getLimit() const { return limit_; }
    std::size_t getOffset() const { return offset_; }

private:
    std::vector<std::pair<hamdb::OrderByDirection, std::unique_ptr<hamdb::Expression>>> order_bys_;
    std::size_t limit_;
    std::size_t offset_;
};

} // namespace hamdb::planner
