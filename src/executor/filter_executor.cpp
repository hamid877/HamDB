#include "executor/filter_executor.hpp"

namespace hamdb {

FilterExecutor::FilterExecutor(std::unique_ptr<AbstractExecutor> child, std::unique_ptr<Expression> predicate)
    : child_(std::move(child)), predicate_(std::move(predicate)) {}

void FilterExecutor::init() {
    child_->init();
}

bool FilterExecutor::next(Tuple* tuple, RID* rid) {
    while (child_->next(tuple, rid)) {
        Value val = predicate_->evaluate(*tuple, outputSchema());
        if (!val.isNull() && val.getAsBoolean()) {
            return true;
        }
    }
    return false;
}

const Schema& FilterExecutor::outputSchema() const {
    return child_->outputSchema();
}

} // namespace hamdb
