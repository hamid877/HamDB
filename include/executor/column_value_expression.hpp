#pragma once

#include "executor/expression.hpp"
#include <cstdint>
#include <optional>

namespace hamdb
{

    enum class TupleSource
    {
        Left,
        Right
    };

    class ColumnValueExpression : public Expression
    {
    public:
        explicit ColumnValueExpression(uint32_t col_idx) : col_idx_(col_idx) {}
        ColumnValueExpression(uint32_t col_idx, TupleSource tuple_source)
            : col_idx_(col_idx), tuple_source_(tuple_source)
        {
        }

        [[nodiscard]] std::unique_ptr<Expression> clone() const override
        {
            if (tuple_source_) {
                return std::make_unique<ColumnValueExpression>(col_idx_, *tuple_source_);
            }
            return std::make_unique<ColumnValueExpression>(col_idx_);
        }

        [[nodiscard]] Value evaluate(const Tuple& tuple, const Schema& schema) const override;
        [[nodiscard]] uint32_t getColIdx() const { return col_idx_; }

        [[nodiscard]] Value evaluateJoin(const Tuple* left_tuple, const Schema* left_schema,
                                         const Tuple* right_tuple,
                                         const Schema* right_schema) const override;

    private:
        uint32_t col_idx_;
        std::optional<TupleSource> tuple_source_{std::nullopt};
    };

} // namespace hamdb
