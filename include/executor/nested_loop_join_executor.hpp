#pragma once

#include "executor/abstract_executor.hpp"
#include "executor/expression.hpp"
#include <memory>

namespace hamdb
{

    class NestedLoopJoinExecutor : public AbstractExecutor
    {
    public:
        NestedLoopJoinExecutor(std::unique_ptr<AbstractExecutor> left_child,
                               std::unique_ptr<AbstractExecutor> right_child,
                               std::unique_ptr<Expression> predicate);

        void init() override;
        bool next(Tuple* tuple, RID* rid) override;
        const Schema& outputSchema() const override;

    private:
        std::unique_ptr<AbstractExecutor> left_child_;
        std::unique_ptr<AbstractExecutor> right_child_;
        std::unique_ptr<Expression> predicate_;
        Schema output_schema_;

        bool left_has_more_;
        Tuple current_left_tuple_;
        RID current_left_rid_;
    };

} // namespace hamdb
