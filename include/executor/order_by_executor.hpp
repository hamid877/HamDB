#pragma once

#include "executor/abstract_executor.hpp"
#include "planner/order_by_plan.hpp"
#include <memory>
#include <utility>
#include <vector>

namespace hamdb {

    class OrderByExecutor : public AbstractExecutor {
    public:
        OrderByExecutor(std::unique_ptr<AbstractExecutor> child,
                        std::vector<std::pair<OrderByDirection, std::unique_ptr<Expression>>> order_bys);

        void init() override;
        bool next(Tuple* tuple, RID* rid) override;
        const Schema& outputSchema() const override;

    private:
        std::unique_ptr<AbstractExecutor> child_;
        std::vector<std::pair<OrderByDirection, std::unique_ptr<Expression>>> order_bys_;

        std::vector<std::pair<Tuple, RID>> sorted_tuples_;
        size_t current_idx_;
    };

} // namespace hamdb
