#pragma once

#include "executor/value.hpp"

namespace hamdb {

struct ParameterValue {
    TypeId type;
    Value value;

    ParameterValue() : type(TypeId::Null) {}
    ParameterValue(TypeId type, Value value) : type(type), value(std::move(value)) {}
};

} // namespace hamdb
