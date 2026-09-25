#pragma once

#include "executor/value.hpp"
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace hamdb {

enum class AggregateType { CountStar, Count, Sum, Min, Max, Avg };

struct AggregateKey {
    std::vector<Value> group_bys_;

    bool operator==(const AggregateKey& other) const {
        if (group_bys_.size() != other.group_bys_.size()) {
            return false;
        }
        for (size_t i = 0; i < group_bys_.size(); ++i) {
            // Nulls should compare as equal for grouping
            if (group_bys_[i].isNull() && other.group_bys_[i].isNull()) {
                continue;
            }
            if (group_bys_[i].isNull() || other.group_bys_[i].isNull()) {
                return false;
            }
            if (!group_bys_[i].compareEquals(other.group_bys_[i]).getAsBoolean()) {
                return false;
            }
        }
        return true;
    }
};

} // namespace hamdb

namespace std {
template <>
struct hash<hamdb::AggregateKey> {
    std::size_t operator()(const hamdb::AggregateKey& agg_key) const {
        std::size_t hash_val = 0;
        for (const auto& val : agg_key.group_bys_) {
            std::size_t h = 0;
            if (!val.isNull()) {
                if (val.getType() == hamdb::TypeId::Integer) {
                    h = std::hash<int32_t>()(val.getAsInteger());
                } else if (val.getType() == hamdb::TypeId::Boolean) {
                    h = std::hash<bool>()(val.getAsBoolean());
                } else if (val.getType() == hamdb::TypeId::Varchar) {
                    h = std::hash<std::string>()(val.getAsVarchar());
                }
            }
            hash_val ^= h + 0x9e3779b9 + (hash_val << 6) + (hash_val >> 2);
        }
        return hash_val;
    }
};
} // namespace std

namespace hamdb {

struct AggregateState {
    Value value_{}; // Initialized to Null implicitly
    int32_t count_{0};
};

struct AggregateValue {
    std::vector<Value> aggregates_;
};

class AggregationHashTable {
public:
    explicit AggregationHashTable(const std::vector<AggregateType>& agg_types);

    void insertCombine(const AggregateKey& agg_key, const AggregateValue& agg_val);
    void clear();

    class Iterator {
    public:
        Iterator(std::unordered_map<AggregateKey, std::vector<AggregateState>>::const_iterator iter,
                 std::unordered_map<AggregateKey, std::vector<AggregateState>>::const_iterator end,
                 const std::vector<AggregateType>& agg_types);

        bool isEnd() const;
        void next();
        
        const AggregateKey& key() const;
        std::vector<Value> value() const;

    private:
        std::unordered_map<AggregateKey, std::vector<AggregateState>>::const_iterator iter_;
        std::unordered_map<AggregateKey, std::vector<AggregateState>>::const_iterator end_;
        const std::vector<AggregateType>& agg_types_;
    };

    Iterator begin() const;
    Iterator end() const;

private:
    std::vector<AggregateState> initializeState(const AggregateValue& agg_val) const;
    void combineState(std::vector<AggregateState>& states, const AggregateValue& agg_val) const;

    std::vector<AggregateType> agg_types_;
    std::unordered_map<AggregateKey, std::vector<AggregateState>> ht_;
};

} // namespace hamdb
