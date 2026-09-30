#pragma once

#include "executor/abstract_executor.hpp"
#include "planner/top_k_plan.hpp"
#include "planner/order_by_plan.hpp"
#include <memory>
#include <utility>
#include <vector>

namespace hamdb {

    class TopKExecutor : public AbstractExecutor {
    public:
        TopKExecutor(std::unique_ptr<AbstractExecutor> child,
                     std::vector<std::pair<OrderByDirection, std::unique_ptr<Expression>>> order_bys,
                     std::size_t limit,
                     std::size_t offset);

        void init() override;
        bool next(Tuple* tuple, RID* rid) override;
        const Schema& outputSchema() const override;

    private:
        std::unique_ptr<AbstractExecutor> child_;
        std::vector<std::pair<OrderByDirection, std::unique_ptr<Expression>>> order_bys_;
        std::size_t limit_;
        std::size_t offset_;

        std::vector<std::pair<Tuple, RID>> top_k_tuples_;
        size_t current_idx_;
    };

} // namespace hamdb
