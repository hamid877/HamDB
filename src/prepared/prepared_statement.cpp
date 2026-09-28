#include "prepared/prepared_statement.hpp"

namespace hamdb {

PreparedStatement::PreparedStatement(std::string name, 
                                     std::unique_ptr<planner::LogicalPlanNode> logical_plan,
                                     std::unique_ptr<planner::AbstractPlanNode> physical_plan,
                                     std::vector<TypeId> parameter_types)
    : name_(std::move(name)), 
      logical_plan_(std::move(logical_plan)), 
      physical_plan_(std::move(physical_plan)),
      parameter_types_(std::move(parameter_types)) {}

} // namespace hamdb
