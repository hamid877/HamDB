#pragma once

/// @file disk_manager.hpp
/// @brief Manages low-level read/write operations against a database file.

#include "common/constants.hpp"
#include "common/enums.hpp"
#include "storage/page.hpp"
#include <filesystem>
#include <fstream>
#include <string>

namespace hamdb
{

    /**
     * @brief Manages low-level read and write operations against a database file.
     *
     * @c DiskManager abstracts direct file I/O.  It opens (or creates) a single
     * database file and provides page-granular read/write operations.  All
     * addresses are expressed in logical @c PageId values; the manager translates
     * them to byte offsets internally.
     *
     * Lifecycle:
     *  - Construct with a filesystem path (no I/O performed yet).
     *  - Call @c createDatabase() to initialise a new file, or
     *    @c openDatabase() to attach to an existing one.
     *  - Call @c closeDatabase() (or let the destructor run) to flush and close.
     *
     * @note DiskManager is not thread-safe.  Callers must serialise access.
     * @note DiskManager follows RAII: the destructor calls @c closeDatabase().
     */
    class DiskManager
    {
    public:
        // ── Construction / Destruction ────────────────────────────────────────

        /**
         * @brief Construct a DiskManager bound to @p path.
         *
         * No file I/O is performed here.  Call @c createDatabase() or
         * @c openDatabase() after construction.
         *
         * @param path Filesystem path for the @c .hamdb file.
         */
        explicit DiskManager(const std::filesystem::path& path);

        /// Destructor — calls @c closeDatabase() if the file is open.
        ~DiskManager();

        // Non-copyable, movable.
        DiskManager(const DiskManager&) = delete;
        DiskManager& operator=(const DiskManager&) = delete;
        DiskManager(DiskManager&&) = default;
        DiskManager& operator=(DiskManager&&) = default;

        // ── Lifecycle ─────────────────────────────────────────────────────────

        /**
         * @brief Create a new database file at the configured path.
         *
         * - Returns @c Status::AlreadyExists if the file already exists.
         * - Writes one 4096-byte page initialised to zero.
         * - Encodes @c DatabaseMetadata into the first 64 bytes of that page.
         * - Flushes and closes the file before returning.
         *
         * @return @c Status::Ok on success, @c Status::AlreadyExists if the file
         *         exists, @c Status::IoError on any other I/O failure.
         */
        [[nodiscard]] Status createDatabase();

        /**
         * @brief Open an existing database file at the configured path.
         *
         * - Verifies the file exists and is at least @c kPageSize bytes.
         * - Reads page 0 and validates the magic number and version.
         * - Caches the page count from the @c DatabaseMetadata.
         *
         * @return @c Status::Ok on success,
         *         @c Status::NotFound if the file does not exist,
         *         @c Status::Corruption if magic/version check fails,
         *         @c Status::IoError on read failure.
         */
        [[nodiscard]] Status openDatabase();

        /**
         * @brief Flush and close the database file.
         *
         * Safe to call even when the file is not open (no-op in that case).
         * Resets @c is_open_ and @c page_count_ to their initial values.
         *
         * @return @c Status::Ok on success, @c Status::IoError on flush failure.
         */
        [[nodiscard]] Status closeDatabase();

        // ── Core I/O ─────────────────────────────────────────────────────────

        /**
         * @brief Read one page from disk into @p page.
         *
         * @param page_id  Logical page number.
         * @param[out] page Destination page object.
         * @return @c Status::Ok, @c Status::NotFound, or @c Status::IoError.
         */
        [[nodiscard]] Status readPage(PageId page_id, Page& page);

        /**
         * @brief Write @p page to disk at the position of @p page_id.
         *
         * @param page_id Logical page number.
         * @param page    Source page object.
         * @return @c Status::Ok or @c Status::IoError.
         */
        [[nodiscard]] Status writePage(PageId page_id, const Page& page);

        /**
         * @brief Extend the file by one page and return the new page's ID.
         *
         * @param[out] new_page_id Receives the allocated page ID.
         * @return @c Status::Ok or @c Status::IoError.
         */
        [[nodiscard]] Status allocatePage(PageId& new_page_id);

        // ── Metadata ─────────────────────────────────────────────────────────

        /// Return the total number of pages in the database file.
        [[nodiscard]] std::size_t pageCount() const;

        /// Return the filesystem path of the managed database file.
        [[nodiscard]] const std::filesystem::path& filePath() const;

        /// Return true if a database file is currently open.
        [[nodiscard]] bool isOpen() const;

        /// Flush outstanding OS buffers to disk.
        [[nodiscard]] Status sync();

    private:
        std::filesystem::path path_;       ///< Path to the .hamdb file.
        std::fstream          stream_;     ///< Binary file stream.
        std::size_t           page_count_; ///< Cached page count from metadata.
        bool                  is_open_;    ///< True when a file is open.
    };

} // namespace hamdb
