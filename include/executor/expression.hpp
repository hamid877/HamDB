#pragma once

#include "catalog/schema.hpp"
#include "executor/value.hpp"
#include "storage/tuple.hpp"
#include <memory>
#include <vector>

namespace hamdb
{

    class Expression
    {
    public:
        virtual ~Expression() = default;

        [[nodiscard]] virtual std::unique_ptr<Expression> clone() const = 0;

        [[nodiscard]] virtual Value evaluate(const Tuple& tuple, const Schema& schema) const = 0;

        [[nodiscard]] virtual Value evaluateJoin(const Tuple* left_tuple, const Schema* left_schema,
                                                 const Tuple* right_tuple,
                                                 const Schema* right_schema) const
        {
            return evaluate(left_tuple ? *left_tuple : *right_tuple,
                            left_schema ? *left_schema : *right_schema);
        }

        virtual void bindParameters(const std::vector<Value>& params)
        {
            for (auto& child : children_) {
                if (child) {
                    child->bindParameters(params);
                }
            }
        }

        [[nodiscard]] const std::vector<std::unique_ptr<Expression>>& getChildren() const
        {
            return children_;
        }

        [[nodiscard]] std::vector<std::unique_ptr<Expression>>& getMutableChildren()
        {
            return children_;
        }

    protected:
        std::vector<std::unique_ptr<Expression>> children_;
    };

} // namespace hamdb
