#pragma once

#include "executor/expression.hpp"
#include "executor/value.hpp"
#include "executor/constant_expression.hpp"
#include "executor/column_value_expression.hpp"
#include "executor/arithmetic_expression.hpp"
#include "executor/comparison_expression.hpp"
#include "executor/logical_expression.hpp"
#include <memory>
#include <string>
#include <vector>

namespace hamdb::binder {

enum class BoundExpressionType {
    CONSTANT,
    COLUMN_REF,
    ARITHMETIC,
    COMPARISON,
    LOGICAL
};

class BoundExpression {
public:
    virtual ~BoundExpression() = default;
    virtual BoundExpressionType getBoundType() const = 0;
    virtual TypeId getType() const = 0;
    virtual const hamdb::Expression* getExpr() const = 0;
    virtual std::unique_ptr<hamdb::Expression> takeExpr() = 0;
};

class BoundConstant : public BoundExpression {
public:
    BoundConstant(std::unique_ptr<hamdb::ConstantExpression> expr, TypeId type)
        : expr_(std::move(expr)), type_(type) {}

    BoundExpressionType getBoundType() const override { return BoundExpressionType::CONSTANT; }
    TypeId getType() const override { return type_; }
    const hamdb::Expression* getExpr() const override { return expr_.get(); }
    std::unique_ptr<hamdb::Expression> takeExpr() override { return std::move(expr_); }

private:
    std::unique_ptr<hamdb::ConstantExpression> expr_;
    TypeId type_;
};

class BoundColumnRef : public BoundExpression {
public:
    BoundColumnRef(std::unique_ptr<hamdb::ColumnValueExpression> expr, TypeId type, std::string table_name, std::string column_name)
        : expr_(std::move(expr)), type_(type), table_name_(std::move(table_name)), column_name_(std::move(column_name)) {}

    BoundExpressionType getBoundType() const override { return BoundExpressionType::COLUMN_REF; }
    TypeId getType() const override { return type_; }
    const hamdb::Expression* getExpr() const override { return expr_.get(); }
    std::unique_ptr<hamdb::Expression> takeExpr() override { return std::move(expr_); }

    const std::string& getTableName() const { return table_name_; }
    const std::string& getColumnName() const { return column_name_; }

private:
    std::unique_ptr<hamdb::ColumnValueExpression> expr_;
    TypeId type_;
    std::string table_name_;
    std::string column_name_;
};

class BoundArithmetic : public BoundExpression {
public:
    BoundArithmetic(std::unique_ptr<hamdb::ArithmeticExpression> expr, TypeId type)
        : expr_(std::move(expr)), type_(type) {}

    BoundExpressionType getBoundType() const override { return BoundExpressionType::ARITHMETIC; }
    TypeId getType() const override { return type_; }
    const hamdb::Expression* getExpr() const override { return expr_.get(); }
    std::unique_ptr<hamdb::Expression> takeExpr() override { return std::move(expr_); }

private:
    std::unique_ptr<hamdb::ArithmeticExpression> expr_;
    TypeId type_;
};

class BoundComparison : public BoundExpression {
public:
    BoundComparison(std::unique_ptr<hamdb::ComparisonExpression> expr, TypeId type)
        : expr_(std::move(expr)), type_(type) {}

    BoundExpressionType getBoundType() const override { return BoundExpressionType::COMPARISON; }
    TypeId getType() const override { return type_; }
    const hamdb::Expression* getExpr() const override { return expr_.get(); }
    std::unique_ptr<hamdb::Expression> takeExpr() override { return std::move(expr_); }

private:
    std::unique_ptr<hamdb::ComparisonExpression> expr_;
    TypeId type_;
};

class BoundLogical : public BoundExpression {
public:
    BoundLogical(std::unique_ptr<hamdb::LogicalExpression> expr, TypeId type)
        : expr_(std::move(expr)), type_(type) {}

    BoundExpressionType getBoundType() const override { return BoundExpressionType::LOGICAL; }
    TypeId getType() const override { return type_; }
    const hamdb::Expression* getExpr() const override { return expr_.get(); }
    std::unique_ptr<hamdb::Expression> takeExpr() override { return std::move(expr_); }

private:
    std::unique_ptr<hamdb::LogicalExpression> expr_;
    TypeId type_;
};

} // namespace hamdb::binder
