#include "executor/hash_join_executor.hpp"

namespace hamdb
{

    HashJoinExecutor::HashJoinExecutor(std::unique_ptr<AbstractExecutor> left_child,
                                       std::unique_ptr<AbstractExecutor> right_child,
                                       std::unique_ptr<Expression> left_key_expr,
                                       std::unique_ptr<Expression> right_key_expr)
        : left_child_(std::move(left_child)), right_child_(std::move(right_child)),
          left_key_expr_(std::move(left_key_expr)), right_key_expr_(std::move(right_key_expr)),
          left_has_more_(false), current_match_idx_(0)
    {
        std::vector<Column> combined_columns;
        const Schema& left_schema = left_child_->outputSchema();
        for (uint32_t i = 0; i < left_schema.getColumnCount(); ++i)
        {
            combined_columns.push_back(left_schema.getColumn(i));
        }
        const Schema& right_schema = right_child_->outputSchema();
        for (uint32_t i = 0; i < right_schema.getColumnCount(); ++i)
        {
            combined_columns.push_back(right_schema.getColumn(i));
        }
        output_schema_ = Schema(combined_columns);
    }

    void HashJoinExecutor::init()
    {
        left_child_->init();
        right_child_->init();
        hash_table_.clear();

        Tuple right_tuple;
        RID right_rid;
        while (right_child_->next(&right_tuple, &right_rid))
        {
            Value key = right_key_expr_->evaluateJoin(nullptr, nullptr, &right_tuple, &right_child_->outputSchema());
            if (!key.isNull())
            {
                hash_table_[key].push_back(right_tuple);
            }
        }

        left_has_more_ = left_child_->next(&current_left_tuple_, &current_left_rid_);
        current_match_idx_ = 0;
    }

    bool HashJoinExecutor::next(Tuple* tuple, RID* rid)
    {
        while (left_has_more_)
        {
            Value left_key = left_key_expr_->evaluateJoin(&current_left_tuple_, &left_child_->outputSchema(), nullptr, nullptr);

            if (!left_key.isNull())
            {
                auto it = hash_table_.find(left_key);
                if (it != hash_table_.end() && current_match_idx_ < it->second.size())
                {
                    const Tuple& right_tuple = it->second[current_match_idx_];
                    current_match_idx_++;

                    std::vector<std::byte> combined_data;
                    combined_data.reserve(current_left_tuple_.size() + right_tuple.size());

                    auto left_data = current_left_tuple_.data();
                    combined_data.insert(combined_data.end(), left_data.begin(), left_data.end());

                    auto right_data = right_tuple.data();
                    combined_data.insert(combined_data.end(), right_data.begin(), right_data.end());

                    *tuple = Tuple(std::span<const std::byte>(combined_data));
                    *rid = current_left_rid_;
                    return true;
                }
            }

            left_has_more_ = left_child_->next(&current_left_tuple_, &current_left_rid_);
            current_match_idx_ = 0;
        }
        return false;
    }

    const Schema& HashJoinExecutor::outputSchema() const
    {
        return output_schema_;
    }

} // namespace hamdb
