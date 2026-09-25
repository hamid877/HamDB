#include "wal/log_manager.hpp"

namespace hamdb
{

    lsn_t LogManager::append(LogRecord& record)
    {
        std::lock_guard<std::mutex> lock(latch_);
        lsn_t lsn = next_lsn_.fetch_add(1);
        record.setLSN(lsn);
        log_buffer_.push_back(record);
        return lsn;
    }

    void LogManager::flush(lsn_t lsn)
    {
        std::lock_guard<std::mutex> lock(latch_);
        if (lsn > persistent_lsn_.load())
        {
            persistent_lsn_.store(lsn);
        }
        // Move flushed records to simulated disk
        std::vector<LogRecord> remaining;
        for (const auto& rec : log_buffer_)
        {
            if (rec.getLSN() > lsn)
            {
                remaining.push_back(rec);
            }
            else
            {
                disk_log_buffer_.push_back(rec);
            }
        }
        log_buffer_ = std::move(remaining);
    }

    void LogManager::flushAll()
    {
        std::lock_guard<std::mutex> lock(latch_);
        if (!log_buffer_.empty())
        {
            lsn_t highest = log_buffer_.back().getLSN();
            if (highest > persistent_lsn_.load())
            {
                persistent_lsn_.store(highest);
            }
            for (const auto& rec : log_buffer_)
            {
                disk_log_buffer_.push_back(rec);
            }
            log_buffer_.clear();
        }
    }

} // namespace hamdb
