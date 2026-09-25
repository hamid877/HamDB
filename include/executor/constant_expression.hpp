#pragma once

#include "executor/expression.hpp"

namespace hamdb
{

    class ConstantExpression : public Expression
    {
    public:
        explicit ConstantExpression(Value value) : value_(std::move(value)) {}

        [[nodiscard]] Value evaluate(const Tuple& /*tuple*/,
                                     const Schema& /*schema*/) const override
        {
            return value_;
        }

    private:
        Value value_;
    };

} // namespace hamdb
