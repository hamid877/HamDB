#include "executor/aggregation_hash_table.hpp"

namespace hamdb
{

    AggregationHashTable::AggregationHashTable(const std::vector<AggregateType>& agg_types)
        : agg_types_(agg_types)
    {
    }

    void AggregationHashTable::clear()
    {
        ht_.clear();
    }

    void AggregationHashTable::insertCombine(const AggregateKey& agg_key,
                                             const AggregateValue& agg_val)
    {
        if (!ht_.contains(agg_key))
        {
            ht_[agg_key] = initializeState(agg_val);
        }
        else
        {
            combineState(ht_[agg_key], agg_val);
        }
    }

    std::vector<AggregateState>
    AggregationHashTable::initializeState(const AggregateValue& agg_val) const
    {
        std::vector<AggregateState> states(agg_types_.size());
        for (size_t i = 0; i < agg_types_.size(); ++i)
        {
            const auto& val = agg_val.aggregates_[i];
            switch (agg_types_[i])
            {
            case AggregateType::CountStar:
                states[i].value_ = Value(static_cast<int32_t>(1));
                break;
            case AggregateType::Count:
                if (!val.isNull())
                {
                    states[i].value_ = Value(static_cast<int32_t>(1));
                }
                else
                {
                    states[i].value_ = Value(static_cast<int32_t>(0));
                }
                break;
            case AggregateType::Sum:
            case AggregateType::Min:
            case AggregateType::Max:
                if (!val.isNull())
                {
                    states[i].value_ = val;
                }
                break;
            case AggregateType::Avg:
                if (!val.isNull())
                {
                    states[i].value_ = val;
                    states[i].count_ = 1;
                }
                break;
            }
        }
        return states;
    }

    void AggregationHashTable::combineState(std::vector<AggregateState>& states,
                                            const AggregateValue& agg_val) const
    {
        for (size_t i = 0; i < agg_types_.size(); ++i)
        {
            const auto& val = agg_val.aggregates_[i];
            switch (agg_types_[i])
            {
            case AggregateType::CountStar:
                states[i].value_ = states[i].value_.add(Value(static_cast<int32_t>(1)));
                break;
            case AggregateType::Count:
                if (!val.isNull())
                {
                    states[i].value_ = states[i].value_.add(Value(static_cast<int32_t>(1)));
                }
                break;
            case AggregateType::Sum:
                if (!val.isNull())
                {
                    if (states[i].value_.isNull())
                    {
                        states[i].value_ = val;
                    }
                    else
                    {
                        states[i].value_ = states[i].value_.add(val);
                    }
                }
                break;
            case AggregateType::Min:
                if (!val.isNull())
                {
                    if (states[i].value_.isNull() ||
                        val.compareLessThan(states[i].value_).getAsBoolean())
                    {
                        states[i].value_ = val;
                    }
                }
                break;
            case AggregateType::Max:
                if (!val.isNull())
                {
                    if (states[i].value_.isNull() ||
                        val.compareGreaterThan(states[i].value_).getAsBoolean())
                    {
                        states[i].value_ = val;
                    }
                }
                break;
            case AggregateType::Avg:
                if (!val.isNull())
                {
                    if (states[i].value_.isNull())
                    {
                        states[i].value_ = val;
                        states[i].count_ = 1;
                    }
                    else
                    {
                        states[i].value_ = states[i].value_.add(val);
                        states[i].count_++;
                    }
                }
                break;
            }
        }
    }

    AggregationHashTable::Iterator::Iterator(
        std::unordered_map<AggregateKey, std::vector<AggregateState>>::const_iterator iter,
        std::unordered_map<AggregateKey, std::vector<AggregateState>>::const_iterator end,
        const std::vector<AggregateType>& agg_types)
        : iter_(iter), end_(end), agg_types_(agg_types)
    {
    }

    bool AggregationHashTable::Iterator::isEnd() const
    {
        return iter_ == end_;
    }

    void AggregationHashTable::Iterator::next()
    {
        if (iter_ != end_)
        {
            ++iter_;
        }
    }

    const AggregateKey& AggregationHashTable::Iterator::key() const
    {
        return iter_->first;
    }

    std::vector<Value> AggregationHashTable::Iterator::value() const
    {
        std::vector<Value> result;
        const auto& states = iter_->second;
        for (size_t i = 0; i < agg_types_.size(); ++i)
        {
            switch (agg_types_[i])
            {
            case AggregateType::CountStar:
            case AggregateType::Count:
            case AggregateType::Sum:
            case AggregateType::Min:
            case AggregateType::Max:
                result.emplace_back(states[i].value_);
                break;
            case AggregateType::Avg:
                if (states[i].value_.isNull())
                {
                    result.emplace_back(); // Null
                }
                else
                {
                    if (states[i].count_ == 0)
                    {
                        result.emplace_back(); // Should not happen
                    }
                    else
                    {
                        // Integer division
                        result.emplace_back(states[i].value_.getAsInteger() / states[i].count_);
                    }
                }
                break;
            }
        }
        return result;
    }

    AggregationHashTable::Iterator AggregationHashTable::begin() const
    {
        return Iterator(ht_.begin(), ht_.end(), agg_types_);
    }

    AggregationHashTable::Iterator AggregationHashTable::end() const
    {
        return Iterator(ht_.end(), ht_.end(), agg_types_);
    }

} // namespace hamdb
