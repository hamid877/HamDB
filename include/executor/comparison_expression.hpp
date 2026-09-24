#pragma once

#include "executor/expression.hpp"
#include <stdexcept>

namespace hamdb {

enum class ComparisonType { Equal, NotEqual, LessThan, LessThanOrEqual, GreaterThan, GreaterThanOrEqual };

class ComparisonExpression : public Expression {
public:
    ComparisonExpression(ComparisonType comp_type, std::unique_ptr<Expression> left, std::unique_ptr<Expression> right)
        : comp_type_(comp_type) {
        children_.push_back(std::move(left));
        children_.push_back(std::move(right));
    }

    [[nodiscard]] Value evaluate(const Tuple& tuple, const Schema& schema) const override {
        Value left = children_[0]->evaluate(tuple, schema);
        Value right = children_[1]->evaluate(tuple, schema);
        
        switch (comp_type_) {
            case ComparisonType::Equal: return left.compareEquals(right);
            case ComparisonType::NotEqual: return left.compareNotEquals(right);
            case ComparisonType::LessThan: return left.compareLessThan(right);
            case ComparisonType::LessThanOrEqual: return left.compareLessThanEquals(right);
            case ComparisonType::GreaterThan: return left.compareGreaterThan(right);
            case ComparisonType::GreaterThanOrEqual: return left.compareGreaterThanEquals(right);
        }
        throw std::runtime_error("Unknown comparison type");
    }

private:
    ComparisonType comp_type_;
};

} // namespace hamdb
