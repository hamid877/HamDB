#pragma once

#include "executor/abstract_executor.hpp"
#include "executor/executor_context.hpp"
#include "planner/having_plan.hpp"

namespace hamdb::executor {

class HavingExecutor : public AbstractExecutor {
public:
    HavingExecutor(ExecutorContext *context,
               const planner::HavingPlan *plan,
               std::unique_ptr<AbstractExecutor> child);

    void init() override;
    bool next(Tuple* tuple, RID* rid) override;
    const Schema& outputSchema() const override;

private:
    const planner::HavingPlan* plan_;
    std::unique_ptr<AbstractExecutor> child_;
};

} // namespace hamdb::executor
