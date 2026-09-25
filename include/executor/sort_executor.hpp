#pragma once

#include "executor/abstract_executor.hpp"
#include "executor/expression.hpp"
#include <memory>
#include <utility>
#include <vector>

namespace hamdb
{

    enum class OrderByType
    {
        ASC,
        DESC
    };

    class SortExecutor : public AbstractExecutor
    {
    public:
        SortExecutor(std::unique_ptr<AbstractExecutor> child,
                     std::vector<std::pair<OrderByType, std::unique_ptr<Expression>>> order_bys);

        void init() override;
        bool next(Tuple* tuple, RID* rid) override;
        const Schema& outputSchema() const override;

    private:
        std::unique_ptr<AbstractExecutor> child_;
        std::vector<std::pair<OrderByType, std::unique_ptr<Expression>>> order_bys_;

        std::vector<std::pair<Tuple, RID>> sorted_tuples_;
        size_t current_idx_;
    };

} // namespace hamdb
