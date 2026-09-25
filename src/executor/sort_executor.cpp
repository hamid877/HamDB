#include "executor/sort_executor.hpp"
#include <algorithm>

namespace hamdb
{

    SortExecutor::SortExecutor(
        std::unique_ptr<AbstractExecutor> child,
        std::vector<std::pair<OrderByType, std::unique_ptr<Expression>>> order_bys)
        : child_(std::move(child)), order_bys_(std::move(order_bys)), current_idx_(0)
    {
    }

    void SortExecutor::init()
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

        auto comparator = [this](const std::pair<Tuple, RID>& a, const std::pair<Tuple, RID>& b)
        {
            for (const auto& order_by : order_bys_)
            {
                Value val_a = order_by.second->evaluate(a.first, child_->outputSchema());
                Value val_b = order_by.second->evaluate(b.first, child_->outputSchema());

                if (val_a.compareEquals(val_b).getAsBoolean())
                {
                    continue;
                }

                if (order_by.first == OrderByType::ASC)
                {
                    return val_a.compareLessThan(val_b).getAsBoolean();
                }
                else
                {
                    return val_a.compareGreaterThan(val_b).getAsBoolean();
                }
            }
            return false; // Equal, so not less than
        };

        std::sort(sorted_tuples_.begin(), sorted_tuples_.end(), comparator);
    }

    bool SortExecutor::next(Tuple* tuple, RID* rid)
    {
        if (current_idx_ < sorted_tuples_.size())
        {
            *tuple = sorted_tuples_[current_idx_].first;
            *rid = sorted_tuples_[current_idx_].second;
            current_idx_++;
            return true;
        }
        return false;
    }

    const Schema& SortExecutor::outputSchema() const
    {
        return child_->outputSchema();
    }

} // namespace hamdb
