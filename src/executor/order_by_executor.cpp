#include "executor/order_by_executor.hpp"
#include <algorithm>

namespace hamdb {

    OrderByExecutor::OrderByExecutor(
        std::unique_ptr<AbstractExecutor> child,
        std::vector<std::pair<OrderByDirection, std::unique_ptr<Expression>>> order_bys)
        : child_(std::move(child)), order_bys_(std::move(order_bys)), current_idx_(0)
    {
    }

    void OrderByExecutor::init()
    {
        child_->init();
        sorted_tuples_.clear();
        current_idx_ = 0;

        Tuple tuple;
        RID rid;
        while (child_->next(&tuple, &rid))
        {
            sorted_tuples_.emplace_back(tuple, rid);
        }

        auto comparator = [this](const std::pair<Tuple, RID>& a, const std::pair<Tuple, RID>& b) {
            for (const auto& order_by : order_bys_)
            {
                Value val_a = order_by.second->evaluate(a.first, child_->outputSchema());
                Value val_b = order_by.second->evaluate(b.first, child_->outputSchema());

                if (val_a.compareEquals(val_b).getAsBoolean())
                {
                    continue;
                }

                if (order_by.first == OrderByDirection::ASC)
                {
                    return val_a.compareLessThan(val_b).getAsBoolean();
                }
                else
                {
                    return val_a.compareGreaterThan(val_b).getAsBoolean();
                }
            }
            return false;
        };

        std::stable_sort(sorted_tuples_.begin(), sorted_tuples_.end(), comparator);
    }

    bool OrderByExecutor::next(Tuple* tuple, RID* rid)
    {
        if (current_idx_ >= sorted_tuples_.size())
        {
            return false;
        }

        *tuple = sorted_tuples_[current_idx_].first;
        *rid = sorted_tuples_[current_idx_].second;
        current_idx_++;
        return true;
    }

    const Schema& OrderByExecutor::outputSchema() const
    {
        return child_->outputSchema();
    }

} // namespace hamdb
