#pragma once

#include "executor/expression.hpp"
#include <stdexcept>

namespace hamdb {

class ParameterExpression : public Expression {
public:
    explicit ParameterExpression(size_t param_idx) : param_idx_(param_idx) {}

    [[nodiscard]] std::unique_ptr<Expression> clone() const override {
        auto param = std::make_unique<ParameterExpression>(param_idx_);
        param->value_ = value_;
        param->bound_ = bound_;
        return param;
    }

    [[nodiscard]] Value evaluate(const Tuple& /*tuple*/, const Schema& /*schema*/) const override {
        if (!bound_) {
            throw std::runtime_error("Parameter value not bound");
        }
        return value_;
    }

    void bindParameters(const std::vector<Value>& params) override {
        if (param_idx_ < params.size()) {
            value_ = params[param_idx_];
            bound_ = true;
        }
    }

private:
    size_t param_idx_;
    Value value_;
    bool bound_{false};
};

} // namespace hamdb
