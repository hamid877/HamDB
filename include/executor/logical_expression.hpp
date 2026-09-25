#pragma once

#include "executor/expression.hpp"
#include <stdexcept>

namespace hamdb {

enum class LogicalType { And, Or, Not };

class LogicalExpression : public Expression {
public:
    LogicalExpression(LogicalType logic_type, std::unique_ptr<Expression> child)
        : logic_type_(logic_type) {
        if (logic_type_ != LogicalType::Not) {
            throw std::invalid_argument("Expected NOT logic type for single child");
        }
        children_.push_back(std::move(child));
    }

    LogicalExpression(LogicalType logic_type, std::unique_ptr<Expression> left, std::unique_ptr<Expression> right)
        : logic_type_(logic_type) {
        if (logic_type_ == LogicalType::Not) {
            throw std::invalid_argument("Expected AND or OR logic type for two children");
        }
        children_.push_back(std::move(left));
        children_.push_back(std::move(right));
    }

    [[nodiscard]] Value evaluate(const Tuple& tuple, const Schema& schema) const override {
        return evaluateJoin(&tuple, &schema, nullptr, nullptr);
    }

    [[nodiscard]] Value evaluateJoin(const Tuple* left_tuple, const Schema* left_schema, const Tuple* right_tuple, const Schema* right_schema) const override {
        if (logic_type_ == LogicalType::Not) {
            Value child_val = children_[0]->evaluateJoin(left_tuple, left_schema, right_tuple, right_schema);
            if (child_val.isNull()) return Value();
            return Value(!child_val.getAsBoolean());
        }

        Value left = children_[0]->evaluateJoin(left_tuple, left_schema, right_tuple, right_schema);
        Value right = children_[1]->evaluateJoin(left_tuple, left_schema, right_tuple, right_schema);
        
        if (left.isNull() || right.isNull()) return Value();

        switch (logic_type_) {
            case LogicalType::And: return Value(left.getAsBoolean() && right.getAsBoolean());
            case LogicalType::Or: return Value(left.getAsBoolean() || right.getAsBoolean());
            default: throw std::runtime_error("Unknown logical type");
        }
    }

private:
    LogicalType logic_type_;
};

} // namespace hamdb
