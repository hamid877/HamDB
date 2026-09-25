#pragma once

#include "buffer/buffer_pool_manager.hpp"
#include "wal/log_manager.hpp"
#include "wal/log_record.hpp"
#include <unordered_set>
#include <vector>

namespace hamdb
{

    /**
     * @brief Recovers the database state from WAL logs.
     */
    class RecoveryManager
    {
    public:
        RecoveryManager(BufferPoolManager& bpm, LogManager& log_manager);
        ~RecoveryManager() = default;

        /**
         * @brief Performs the full ARIES-like crash recovery.
         *
         * 1. Analyze: determines active transactions.
         * 2. Redo: replays history to restore system state.
         * 3. Undo: rolls back incomplete transactions.
         */
        void recover();

    private:
        void analyze();
        void redo();
        void undo();

        BufferPoolManager& bpm_;
        LogManager& log_manager_;
        std::unordered_set<txn_id_t> active_txns_;
        std::vector<LogRecord> log_records_;
    };

} // namespace hamdb
