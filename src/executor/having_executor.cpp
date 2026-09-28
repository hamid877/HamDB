#include "executor/having_executor.hpp"

namespace hamdb::executor {

HavingExecutor::HavingExecutor(
    ExecutorContext *context,
    const HavingPlanNode *plan,
    std::unique_ptr<AbstractExecutor> child)
    : plan_(plan),
      child_(std::move(child)) {
    (void)context;   // ExecutorContext reserved for future use.
}

void HavingExecutor::init() {
    child_->init();
}

bool HavingExecutor::next(Tuple* tuple, RID* rid) {
    while (child_->next(tuple, rid)) {
        if (!predicate_) {
            return true;
        }
        Value val = predicate_->evaluate(*tuple, child_->outputSchema());
        if (val.getType() != TypeId::Null && val.getAsBoolean()) {
            return true;
        }
    }
    return false;
}

const Schema& HavingExecutor::outputSchema() const {
    return child_->outputSchema();
}

} // namespace hamdb::executor
