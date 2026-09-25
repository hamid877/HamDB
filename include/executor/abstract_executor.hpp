#pragma once

#include "catalog/schema.hpp"
#include "storage/rid.hpp"
#include "storage/tuple.hpp"

namespace hamdb
{

    class AbstractExecutor
    {
    public:
        virtual ~AbstractExecutor() = default;

        virtual void init() = 0;
        virtual bool next(Tuple* tuple, RID* rid) = 0;
        virtual const Schema& outputSchema() const = 0;
    };

} // namespace hamdb
