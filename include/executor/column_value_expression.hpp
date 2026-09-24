#pragma once

#include "executor/expression.hpp"
#include <cstdint>

namespace hamdb {

class ColumnValueExpression : public Expression {
public:
    explicit ColumnValueExpression(uint32_t col_idx) : col_idx_(col_idx) {}

    [[nodiscard]] Value evaluate(const Tuple& tuple, const Schema& schema) const override;

private:
    uint32_t col_idx_;
};

} // namespace hamdb
