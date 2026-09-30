#include "executor/top_k_executor.hpp"
#include <queue>
#include <algorithm>

namespace hamdb {

    TopKExecutor::TopKExecutor(std::unique_ptr<AbstractExecutor> child,
                 std::vector<std::pair<OrderByDirection, std::unique_ptr<Expression>>> order_bys,
                 std::size_t limit,
                 std::size_t offset)
        : child_(std::move(child)), order_bys_(std::move(order_bys)), limit_(limit), offset_(offset), current_idx_(0) {}

    void TopKExecutor::init() {
        child_->init();
        top_k_tuples_.clear();
        current_idx_ = 0;
        
        if (limit_ == 0) {
            return;
        }
        
        std::size_t capacity = limit_ + offset_;

        // Max-heap logic: the priority queue returns the *largest* element at the top.
        // For ASC order, we want to retain the smallest elements. Therefore, the heap
        // should pop the largest elements when size > capacity. So we use a max-heap where
        // `a < b` returns true.
        // For DESC order, we want to retain the largest elements. The heap should pop the 
        // smallest elements when size > capacity. So we use a min-heap where `a > b` returns true.
        auto comparator = [this](const std::pair<Tuple, RID>& a, const std::pair<Tuple, RID>& b) {
            for (const auto& order_by : order_bys_) {
                Value val_a = order_by.second->evaluate(a.first, child_->outputSchema());
                Value val_b = order_by.second->evaluate(b.first, child_->outputSchema());

                if (val_a.compareEquals(val_b).getAsBoolean()) {
                    continue;
                }

                if (order_by.first == OrderByDirection::ASC) {
                    return val_a.compareLessThan(val_b).getAsBoolean();
                } else {
                    return val_a.compareGreaterThan(val_b).getAsBoolean();
                }
            }
            return false; // Equal elements -> not strictly less/greater
        };

        std::priority_queue<std::pair<Tuple, RID>, std::vector<std::pair<Tuple, RID>>, decltype(comparator)> pq(comparator);
        
        Tuple tuple;
        RID rid;
        while (child_->next(&tuple, &rid)) {
            pq.push(std::make_pair(tuple, rid));
            if (pq.size() > capacity) {
                pq.pop();
            }
        }
        
        std::vector<std::pair<Tuple, RID>> temp;
        temp.reserve(pq.size());
        while (!pq.empty()) {
            temp.push_back(pq.top());
            pq.pop();
        }
        
        // Priority queue pops elements in descending order of priority. 
        // We must reverse to get them in sorted order.
        std::reverse(temp.begin(), temp.end());

        // Apply OFFSET and LIMIT
        std::size_t count = 0;
        for (std::size_t i = offset_; i < temp.size() && count < limit_; ++i, ++count) {
            top_k_tuples_.push_back(temp[i]);
        }
    }

    bool TopKExecutor::next(Tuple* tuple, RID* rid) {
        if (current_idx_ >= top_k_tuples_.size()) {
            return false;
        }

        *tuple = top_k_tuples_[current_idx_].first;
        *rid = top_k_tuples_[current_idx_].second;
        current_idx_++;
        return true;
    }

    const Schema& TopKExecutor::outputSchema() const {
        return child_->outputSchema();
    }

} // namespace hamdb
