#pragma once

#include "planner/logical_plan.hpp"
#include "planner/physical_plan.hpp"
#include "executor/expression.hpp"
#include <memory>

namespace hamdb::planner {

class LogicalHavingNode : public LogicalPlanNode {
public:
    LogicalHavingNode(Schema output_schema, std::unique_ptr<hamdb::Expression> predicate)
        : LogicalPlanNode(LogicalPlanType::HAVING, std::move(output_schema)),
          predicate_(std::move(predicate)) {}
    
    const hamdb::Expression* getPredicate() const { return predicate_.get(); }
    std::unique_ptr<hamdb::Expression> takePredicate() { return std::move(predicate_); }
    void setPredicate(std::unique_ptr<hamdb::Expression> predicate) { predicate_ = std::move(predicate); }
private:
    std::unique_ptr<hamdb::Expression> predicate_;
    friend class PhysicalPlanner;
};

class HavingPlan : public AbstractPlanNode {
public:
    HavingPlan(Schema output_schema, std::unique_ptr<hamdb::Expression> predicate)
        : AbstractPlanNode(PhysicalPlanType::HAVING, std::move(output_schema)),
          predicate_(std::move(predicate)) {}
    
    std::unique_ptr<hamdb::Expression>& getPredicate() { return predicate_; }
    const std::unique_ptr<hamdb::Expression>& getPredicate() const { return predicate_; }
private:
    std::unique_ptr<hamdb::Expression> predicate_;
};

} // namespace hamdb::planner
