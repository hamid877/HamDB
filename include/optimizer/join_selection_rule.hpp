#pragma once
#include "optimizer/rule.hpp"
#include <memory>

namespace hamdb::optimizer {

class JoinSelectionRule : public Rule {
public:
    std::unique_ptr<planner::LogicalPlanNode> apply(std::unique_ptr<planner::LogicalPlanNode> plan) override;
    
    std::string name() const override {
        return "JoinSelectionRule";
    }
};

} // namespace hamdb::optimizer
