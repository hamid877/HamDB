#pragma once

#include "planner/physical_plan.hpp"
#include "executor/abstract_executor.hpp"
#include "executor/executor_context.hpp"
#include <memory>

namespace hamdb::planner {

class ExecutorFactory {
public:
    static std::unique_ptr<hamdb::AbstractExecutor> createExecutor(
        hamdb::ExecutorContext* exec_ctx,
        std::unique_ptr<AbstractPlanNode> plan);
};

} // namespace hamdb::planner
