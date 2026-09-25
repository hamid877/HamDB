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

        [[nodiscard]] virtual Value evaluate(const Tuple& tuple, const Schema& schema) const = 0;

        [[nodiscard]] virtual Value evaluateJoin(const Tuple* left_tuple, const Schema* left_schema,
                                                 const Tuple* right_tuple,
                                                 const Schema* right_schema) const
        {
            return evaluate(left_tuple ? *left_tuple : *right_tuple,
                            left_schema ? *left_schema : *right_schema);
        }

        [[nodiscard]] const std::vector<std::unique_ptr<Expression>>& getChildren() const
        {
            return children_;
        }

    protected:
        std::vector<std::unique_ptr<Expression>> children_;
    };

} // namespace hamdb
