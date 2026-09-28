#pragma once

#include "planner/logical_plan.hpp"
#include "planner/physical_plan.hpp"
#include "executor/value.hpp"
#include <string>
#include <vector>
#include <memory>

namespace hamdb {

class PreparedStatement {
public:
    PreparedStatement(std::string name, 
                      std::unique_ptr<planner::LogicalPlanNode> logical_plan,
                      std::unique_ptr<planner::AbstractPlanNode> physical_plan,
                      std::vector<TypeId> parameter_types);

    const std::string& getName() const { return name_; }
    const planner::LogicalPlanNode* getLogicalPlan() const { return logical_plan_.get(); }
    planner::AbstractPlanNode* getPhysicalPlan() { return physical_plan_.get(); }
    const std::vector<TypeId>& getParameterTypes() const { return parameter_types_; }

private:
    std::string name_;
    std::unique_ptr<planner::LogicalPlanNode> logical_plan_;
    std::unique_ptr<planner::AbstractPlanNode> physical_plan_;
    std::vector<TypeId> parameter_types_;
};

} // namespace hamdb
