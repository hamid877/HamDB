#pragma once

#include "storage/tuple.hpp"
#include "storage/rid.hpp"
#include "catalog/schema.hpp"

namespace hamdb {

class AbstractExecutor {
public:
    virtual ~AbstractExecutor() = default;

    virtual void init() = 0;
    virtual bool next(Tuple* tuple, RID* rid) = 0;
    virtual const Schema& outputSchema() const = 0;
};

} // namespace hamdb
