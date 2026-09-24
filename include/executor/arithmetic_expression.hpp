#pragma once

#include "executor/expression.hpp"
#include <stdexcept>

namespace hamdb {

enum class ArithmeticType { Add, Subtract, Multiply, Divide };

class ArithmeticExpression : public Expression {
public:
    ArithmeticExpression(ArithmeticType arith_type, std::unique_ptr<Expression> left, std::unique_ptr<Expression> right)
        : arith_type_(arith_type) {
        children_.push_back(std::move(left));
        children_.push_back(std::move(right));
    }

    [[nodiscard]] Value evaluate(const Tuple& tuple, const Schema& schema) const override {
        Value left = children_[0]->evaluate(tuple, schema);
        Value right = children_[1]->evaluate(tuple, schema);
        
        switch (arith_type_) {
            case ArithmeticType::Add: return left.add(right);
            case ArithmeticType::Subtract: return left.subtract(right);
            case ArithmeticType::Multiply: return left.multiply(right);
            case ArithmeticType::Divide: return left.divide(right);
        }
        throw std::runtime_error("Unknown arithmetic type");
    }

private:
    ArithmeticType arith_type_;
};

} // namespace hamdb
