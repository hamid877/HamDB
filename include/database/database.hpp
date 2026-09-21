#pragma once

/// @file database.hpp
/// @brief Top-level entry point for opening and managing a HamDB database.
///
/// @c Database is the user-facing façade.  Future milestones will wire it
/// to the DiskManager, BufferPool, Catalog, and Executor subsystems.

#include "common/config.hpp"
#include <filesystem>
#include <memory>

namespace hamdb
{

    /**
     * @brief Represents an open HamDB database instance.
     *
     * @c Database is the primary entry point for all engine operations.
     * Callers obtain an instance via @c Database::open() (future milestone) and
     * interact with tables, indexes, and transactions through its methods.
     *
     * The class follows RAII: all resources are released when the object is
     * destroyed (files flushed, locks released, memory freed).
     *
     * @note Database is non-copyable.  Share access through a @c shared_ptr
     *       if multiple owners are needed.
     */
    class Database
    {
    public:
        // ── Construction / Destruction ────────────────────────────────────────────

        /**
         * @brief Construct a Database over an existing database file path.
         *
         * @param path   Filesystem path to the @c .hamdb file.
         * @param config Runtime configuration applied to this instance.
         *
         * @note Full open/create logic is deferred to a future milestone.
         */
        explicit Database(const std::filesystem::path& path, const Config& config = Config{});

        /// Destructor — flushes and closes all underlying resources.
        ~Database();

        // Non-copyable.
        Database(const Database&) = delete;
        Database& operator=(const Database&) = delete;

        // Movable.
        Database(Database&&) = default;
        Database& operator=(Database&&) = default;

    private:
        std::filesystem::path path_; ///< Path to the database file.
        Config config_;              ///< Runtime configuration.
    };

} // namespace hamdb
