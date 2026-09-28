#pragma once
#include "planner/logical_plan.hpp"
#include "planner/physical_plan.hpp"
#include "executor/expression.hpp"
#include <memory>

namespace hamdb::planner {

class LogicalHashJoinNode : public LogicalPlanNode {
public:
    LogicalHashJoinNode(Schema output_schema, std::unique_ptr<hamdb::Expression> left_key_expr, std::unique_ptr<hamdb::Expression> right_key_expr)
        : LogicalPlanNode(LogicalPlanType::HASH_JOIN, std::move(output_schema)),
          left_key_expr_(std::move(left_key_expr)), right_key_expr_(std::move(right_key_expr)) {}
          
    const hamdb::Expression* getLeftKeyExpr() const { return left_key_expr_.get(); }
    const hamdb::Expression* getRightKeyExpr() const { return right_key_expr_.get(); }
    
    std::unique_ptr<hamdb::Expression> takeLeftKeyExpr() { return std::move(left_key_expr_); }
    std::unique_ptr<hamdb::Expression> takeRightKeyExpr() { return std::move(right_key_expr_); }
    
    void setLeftKeyExpr(std::unique_ptr<hamdb::Expression> expr) { left_key_expr_ = std::move(expr); }
    void setRightKeyExpr(std::unique_ptr<hamdb::Expression> expr) { right_key_expr_ = std::move(expr); }
private:
    std::unique_ptr<hamdb::Expression> left_key_expr_;
    std::unique_ptr<hamdb::Expression> right_key_expr_;
    friend class PhysicalPlanner;
};

class HashJoinPlan : public AbstractPlanNode {
public:
    HashJoinPlan(Schema output_schema, std::unique_ptr<hamdb::Expression> left_key_expr, std::unique_ptr<hamdb::Expression> right_key_expr)
        : AbstractPlanNode(PhysicalPlanType::HASH_JOIN, std::move(output_schema)),
          left_key_expr_(std::move(left_key_expr)), right_key_expr_(std::move(right_key_expr)) {}
          
    const std::unique_ptr<hamdb::Expression>& getLeftKeyExpr() const { return left_key_expr_; }
    const std::unique_ptr<hamdb::Expression>& getRightKeyExpr() const { return right_key_expr_; }
    
    std::unique_ptr<hamdb::Expression>& getMutableLeftKeyExpr() { return left_key_expr_; }
    std::unique_ptr<hamdb::Expression>& getMutableRightKeyExpr() { return right_key_expr_; }
private:
    std::unique_ptr<hamdb::Expression> left_key_expr_;
    std::unique_ptr<hamdb::Expression> right_key_expr_;
};

} // namespace hamdb::planner
