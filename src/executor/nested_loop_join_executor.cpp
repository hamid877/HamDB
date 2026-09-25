#include "executor/nested_loop_join_executor.hpp"

namespace hamdb {

NestedLoopJoinExecutor::NestedLoopJoinExecutor(
    std::unique_ptr<AbstractExecutor> left_child,
    std::unique_ptr<AbstractExecutor> right_child,
    std::unique_ptr<Expression> predicate)
    : left_child_(std::move(left_child)),
      right_child_(std::move(right_child)),
      predicate_(std::move(predicate)),
      left_has_more_(false) {
      
    std::vector<Column> combined_columns;
    const Schema& left_schema = left_child_->outputSchema();
    for (uint32_t i = 0; i < left_schema.getColumnCount(); ++i) {
        combined_columns.push_back(left_schema.getColumn(i));
    }
    const Schema& right_schema = right_child_->outputSchema();
    for (uint32_t i = 0; i < right_schema.getColumnCount(); ++i) {
        combined_columns.push_back(right_schema.getColumn(i));
    }
    output_schema_ = Schema(combined_columns);
}

void NestedLoopJoinExecutor::init() {
    left_child_->init();
    right_child_->init();
    left_has_more_ = left_child_->next(&current_left_tuple_, &current_left_rid_);
}

bool NestedLoopJoinExecutor::next(Tuple* tuple, RID* rid) {
    while (left_has_more_) {
        Tuple right_tuple;
        RID right_rid;

        while (right_child_->next(&right_tuple, &right_rid)) {
            bool match = true;
            if (predicate_) {
                Value val = predicate_->evaluateJoin(&current_left_tuple_, &left_child_->outputSchema(),
                                                     &right_tuple, &right_child_->outputSchema());
                if (val.isNull() || !val.getAsBoolean()) {
                    match = false;
                }
            }

            if (match) {
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
        if (left_has_more_) {
            right_child_->init();
        }
    }
    return false;
}

const Schema& NestedLoopJoinExecutor::outputSchema() const {
    return output_schema_;
}

} // namespace hamdb
