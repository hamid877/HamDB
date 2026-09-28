#pragma once

#include "executor/abstract_executor.hpp"
#include "executor/executor_context.hpp"
#include "planner/having_plan.hpp"

namespace hamdb::executor {

class HavingExecutor : public AbstractExecutor {
public:
    HavingExecutor(ExecutorContext* context, std::unique_ptr<AbstractExecutor> child, const hamdb::Expression* predicate);

    void init() override;
    bool next(Tuple* tuple, RID* rid) override;
    const Schema& outputSchema() const override;

private:
    std::unique_ptr<AbstractExecutor> child_;
    const hamdb::Expression* predicate_;
    ExecutorContext* context_;
};

} // namespace hamdb::executor
