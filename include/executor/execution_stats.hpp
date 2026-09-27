#pragma once

#include <cstdint>
#include <chrono>

namespace hamdb::executor {

struct ExecutionStats {
    uint64_t rows_in{0};
    uint64_t rows_out{0};
    std::chrono::microseconds execution_time{0};
};

} // namespace hamdb::executor
