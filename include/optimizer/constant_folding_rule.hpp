#pragma once
#include "optimizer/rule.hpp"
#include "executor/expression.hpp"
#include <memory>

namespace hamdb::optimizer {

class ConstantFoldingRule : public Rule {
public:
    std::unique_ptr<planner::LogicalPlanNode> apply(std::unique_ptr<planner::LogicalPlanNode> plan) override;
    std::string name() const override { return "ConstantFoldingRule"; }

private:
    std::unique_ptr<hamdb::Expression> foldExpression(std::unique_ptr<hamdb::Expression> expr);
};

} // namespace hamdb::optimizer
