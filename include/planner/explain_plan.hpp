#pragma once

#include <string>
#include <vector>

namespace hamdb::planner {

struct ExplainPlan {
    std::string logical_plan;
    std::string optimized_plan;
    std::string physical_plan;
    std::string output_schema;
    std::vector<std::string> optimizer_rules;
};

} // namespace hamdb::planner
