#include "executor/aggregation_executor.hpp"
#include "utils/serializer.hpp"

namespace hamdb {

AggregationExecutor::AggregationExecutor(std::unique_ptr<AbstractExecutor> child,
                                         std::vector<std::unique_ptr<Expression>> group_bys,
                                         std::vector<std::unique_ptr<Expression>> aggregates,
                                         std::vector<AggregateType> agg_types,
                                         Schema output_schema)
    : child_(std::move(child)),
      group_bys_(std::move(group_bys)),
      aggregates_(std::move(aggregates)),
      agg_types_(std::move(agg_types)),
      output_schema_(std::move(output_schema)),
      aht_(agg_types_) {}

void AggregationExecutor::init() {
    child_->init();
    aht_.clear();
    Tuple child_tuple;
    RID child_rid;
    bool has_tuples = false;

    while (child_->next(&child_tuple, &child_rid)) {
        has_tuples = true;
        AggregateKey agg_key;
        agg_key.group_bys_.reserve(group_bys_.size());
        for (const auto& expr : group_bys_) {
            agg_key.group_bys_.push_back(expr->evaluate(child_tuple, child_->outputSchema()));
        }

        AggregateValue agg_val;
        agg_val.aggregates_.reserve(aggregates_.size());
        for (const auto& expr : aggregates_) {
            agg_val.aggregates_.push_back(expr->evaluate(child_tuple, child_->outputSchema()));
        }

        aht_.insertCombine(agg_key, agg_val);
    }

    if (!has_tuples && group_bys_.empty()) {
        is_successful_ = false; // We need to emit the default row
    } else {
        is_successful_ = true; // We just iterate over the hash table
    }

    aht_iterator_ = std::make_unique<AggregationHashTable::Iterator>(aht_.begin());
}

bool AggregationExecutor::next(Tuple* tuple, RID* rid) {
    std::vector<Value> projected_values;

    if (!is_successful_) {
        // Emit the default empty row for aggregates without group by
        is_successful_ = true;
        for (size_t i = 0; i < group_bys_.size(); ++i) {
            projected_values.emplace_back();
        }
        for (size_t i = 0; i < agg_types_.size(); ++i) {
            switch (agg_types_[i]) {
                case AggregateType::CountStar:
                case AggregateType::Count:
                    projected_values.emplace_back(static_cast<int32_t>(0));
                    break;
                case AggregateType::Sum:
                case AggregateType::Min:
                case AggregateType::Max:
                case AggregateType::Avg:
                    // Just put 0 for NULLs since we lack NULL serialization support in this basic version
                    projected_values.emplace_back(static_cast<int32_t>(0));
                    break;
            }
        }
    } else {
        if (aht_iterator_->isEnd()) {
            return false;
        }

        const auto& key = aht_iterator_->key();
        const auto& val = aht_iterator_->value();

        projected_values.reserve(group_bys_.size() + agg_types_.size());
        for (const auto& k : key.group_bys_) {
            projected_values.push_back(k);
        }
        for (const auto& v : val) {
            projected_values.push_back(v);
        }

        aht_iterator_->next();
    }

    std::vector<std::byte> buf(1024);
    Serializer ser(buf);
    for (size_t i = 0; i < projected_values.size(); ++i) {
        auto type = output_schema_.getColumn(i).getType();
        if (type == ColumnType::Integer) {
            int32_t val = projected_values[i].isNull() ? 0 : projected_values[i].getAsInteger();
            (void)ser.writeInt32(val);
        } else if (type == ColumnType::Boolean) {
            bool val = projected_values[i].isNull() ? false : projected_values[i].getAsBoolean();
            (void)ser.writeBool(val);
        } else if (type == ColumnType::Varchar) {
            std::string val = projected_values[i].isNull() ? "" : projected_values[i].getAsVarchar();
            (void)ser.writeString(val);
        }
    }

    *tuple = Tuple(std::span<const std::byte>(buf.data(), ser.position()));
    *rid = RID{}; // Aggregations don't have a specific RID

    return true;
}

const Schema& AggregationExecutor::outputSchema() const {
    return output_schema_;
}

} // namespace hamdb
