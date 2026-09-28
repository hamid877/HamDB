#pragma once
#include "planner/logical_plan.hpp"
#include "planner/physical_plan.hpp"
#include "executor/expression.hpp"
#include <memory>

namespace hamdb::planner {

class LogicalNestedLoopJoinNode : public LogicalPlanNode {
public:
    LogicalNestedLoopJoinNode(Schema output_schema, std::unique_ptr<hamdb::Expression> predicate)
        : LogicalPlanNode(LogicalPlanType::NESTED_LOOP_JOIN, std::move(output_schema)),
          predicate_(std::move(predicate)) {}
          
    const hamdb::Expression* getPredicate() const { return predicate_.get(); }
    std::unique_ptr<hamdb::Expression> takePredicate() { return std::move(predicate_); }
    void setPredicate(std::unique_ptr<hamdb::Expression> expr) { predicate_ = std::move(expr); }
private:
    std::unique_ptr<hamdb::Expression> predicate_;
    friend class PhysicalPlanner;
};

class NestedLoopJoinPlan : public AbstractPlanNode {
public:
    NestedLoopJoinPlan(Schema output_schema, std::unique_ptr<hamdb::Expression> predicate)
        : AbstractPlanNode(PhysicalPlanType::NESTED_LOOP_JOIN, std::move(output_schema)),
          predicate_(std::move(predicate)) {}
          
    const std::unique_ptr<hamdb::Expression>& getPredicate() const { return predicate_; }
    std::unique_ptr<hamdb::Expression>& getPredicate() { return predicate_; }
private:
    std::unique_ptr<hamdb::Expression> predicate_;
};

} // namespace hamdb::planner
