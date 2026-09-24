#pragma once

#include "wal/log_record.hpp"
#include <atomic>
#include <mutex>
#include <vector>
#include <cstdint>

namespace hamdb {

/**
 * @brief Manages write-ahead log records.
 */
class LogManager {
public:
    LogManager() = default;
    ~LogManager() = default;

    lsn_t append(LogRecord& record);
    void flush(lsn_t lsn);
    void flushAll();
    
    [[nodiscard]] auto getPersistentLSN() const noexcept -> lsn_t { return persistent_lsn_.load(); }
    [[nodiscard]] auto getNextLSN() const noexcept -> lsn_t { return next_lsn_.load(); }

private:
    std::atomic<lsn_t> next_lsn_{1};
    std::atomic<lsn_t> persistent_lsn_{kInvalidLSN};
    std::mutex latch_;
    std::vector<LogRecord> log_buffer_;
};

} // namespace hamdb
