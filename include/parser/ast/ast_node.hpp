#pragma once
#include <string>

namespace hamdb::ast {

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual std::string toString() const = 0;
};

} // namespace hamdb::ast
