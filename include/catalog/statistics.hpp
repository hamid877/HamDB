#pragma once

#include "executor/value.hpp"
#include <cstdint>
#include <optional>
#include <vector>

namespace hamdb
{

    class ColumnStatistics
    {
    public:
        std::size_t distinct_count = 0;
        std::size_t null_count = 0;
        std::optional<Value> min_value;
        std::optional<Value> max_value;
    };

    class TableStatistics
    {
    public:
        std::size_t row_count = 0;
        std::vector<ColumnStatistics> column_stats;
    };

} // namespace hamdb
