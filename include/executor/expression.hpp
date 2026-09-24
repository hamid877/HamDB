#pragma once

#include "executor/value.hpp"
#include "storage/tuple.hpp"
#include "catalog/schema.hpp"
#include <memory>
#include <vector>

namespace hamdb {

class Expression {
public:
    virtual ~Expression() = default;
    
    [[nodiscard]] virtual Value evaluate(const Tuple& tuple, const Schema& schema) const = 0;
    
    [[nodiscard]] const std::vector<std::unique_ptr<Expression>>& getChildren() const {
        return children_;
    }
    
protected:
    std::vector<std::unique_ptr<Expression>> children_;
};

} // namespace hamdb
