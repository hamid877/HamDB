#pragma once

#include "executor/abstract_executor.hpp"
#include "executor/expression.hpp"
#include <memory>

namespace hamdb {

class FilterExecutor : public AbstractExecutor {
public:
    FilterExecutor(std::unique_ptr<AbstractExecutor> child, std::unique_ptr<Expression> predicate);

    void init() override;
    bool next(Tuple* tuple, RID* rid) override;
    const Schema& outputSchema() const override;

private:
    std::unique_ptr<AbstractExecutor> child_;
    std::unique_ptr<Expression> predicate_;
};

} // namespace hamdb
