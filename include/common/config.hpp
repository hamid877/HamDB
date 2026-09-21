#pragma once

/// @file config.hpp
/// @brief Runtime-configurable settings for a HamDB instance.
///
/// @c Config is a plain aggregate; fill it and pass it to @c Database::open()
/// (implemented in a future milestone).  Every field has a safe default.

#include "common/constants.hpp"

namespace hamdb
{

    /**
     * @brief Aggregates all tunable parameters for a HamDB database instance.
     *
     * Consumers construct a @c Config with the defaults and override only the
     * fields they need:
     * @code
     * hamdb::Config cfg;
     * cfg.page_size = 8192;
     * hamdb::Database db = hamdb::Database::open("mydb.hamdb", cfg);
     * @endcode
     */
    struct Config
    {
        /// Size of a single page in bytes.  Must be a power of two ≥ 512.
        std::size_t page_size = kPageSize;

        /// Maximum number of pages the buffer pool may hold in memory at once.
        std::size_t buffer_pool_capacity = 64;

        /// If true, the database file is opened in read-only mode.
        bool read_only = false;

        /// If true, the WAL is flushed on every transaction commit (safest mode).
        bool sync_on_commit = true;
    };

} // namespace hamdb
