#pragma once

#include "executor/abstract_executor.hpp"
#include "executor/expression.hpp"
#include <memory>
#include <unordered_map>
#include <vector>

namespace hamdb
{

    struct ValueEqual {
        bool operator()(const Value& lhs, const Value& rhs) const {
            if (lhs.isNull() || rhs.isNull()) return false;
            return lhs.compareEquals(rhs).getAsBoolean();
        }
    };

    struct ValueHash {
        std::size_t operator()(const Value& val) const {
            if (val.isNull()) return 0;
            switch (val.getType()) {
                case TypeId::Integer: return std::hash<int32_t>{}(val.getAsInteger());
                case TypeId::Boolean: return std::hash<bool>{}(val.getAsBoolean());
                case TypeId::Varchar: return std::hash<std::string>{}(val.getAsVarchar());
                default: return 0;
            }
        }
    };

    class HashJoinExecutor : public AbstractExecutor
    {
    public:
        HashJoinExecutor(std::unique_ptr<AbstractExecutor> left_child,
                         std::unique_ptr<AbstractExecutor> right_child,
                         std::unique_ptr<Expression> left_key_expr,
                         std::unique_ptr<Expression> right_key_expr);

        void init() override;
        bool next(Tuple* tuple, RID* rid) override;
        const Schema& outputSchema() const override;

    private:
        std::unique_ptr<AbstractExecutor> left_child_;
        std::unique_ptr<AbstractExecutor> right_child_;
        std::unique_ptr<Expression> left_key_expr_;
        std::unique_ptr<Expression> right_key_expr_;
        Schema output_schema_;

        std::unordered_map<Value, std::vector<Tuple>, ValueHash, ValueEqual> hash_table_;
        bool left_has_more_;
        Tuple current_left_tuple_;
        RID current_left_rid_;
        size_t current_match_idx_;
    };

} // namespace hamdb
