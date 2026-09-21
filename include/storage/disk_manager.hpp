#pragma once

/// @file disk_manager.hpp
/// @brief Manages low-level read/write operations against a database file.

#include "common/constants.hpp"
#include "common/enums.hpp"
#include "storage/page.hpp"
#include <filesystem>
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
     * Responsibilities:
     *  - Open / create / close a database file.
     *  - Read a single page from disk into a @c Page object.
     *  - Write a single @c Page from memory to disk.
     *  - Allocate a new page at the end of the file.
     *  - Report the current page count.
     *
     * @c DiskManager is not thread-safe.  The buffer pool manager (a future
     * milestone) is expected to hold an exclusive latch before calling into it.
     *
     * @note DiskManager follows RAII: the file is closed when the object is
     *       destroyed.
     */
    class DiskManager
    {
    public:
        // ── Construction / Destruction ────────────────────────────────────────────

        /**
         * @brief Open or create a database file.
         *
         * If the file does not exist it is created and initialised with zero pages.
         * If the file exists it is opened in read/write mode.
         *
         * @param path Path to the database file.
         * @throws std::runtime_error if the file cannot be opened or created.
         */
        explicit DiskManager(const std::filesystem::path& path);

        ~DiskManager();

        // DiskManager manages an OS file handle — non-copyable, movable.
        DiskManager(const DiskManager&) = delete;
        DiskManager& operator=(const DiskManager&) = delete;
        DiskManager(DiskManager&&) = default;
        DiskManager& operator=(DiskManager&&) = default;

        // ── Core I/O ──────────────────────────────────────────────────────────────

        /**
         * @brief Read one page from disk.
         *
         * @param page_id  The logical ID of the page to read.
         * @param[out] page Target page object to populate.
         * @return @c Status::Ok on success, @c Status::IoError on failure,
         *         @c Status::NotFound if @p page_id is out of range.
         */
        [[nodiscard]] Status readPage(PageId page_id, Page& page);

        /**
         * @brief Write one page to disk.
         *
         * @param page_id The logical ID that determines the file offset.
         * @param page    The page whose contents will be flushed.
         * @return @c Status::Ok on success, @c Status::IoError on failure.
         */
        [[nodiscard]] Status writePage(PageId page_id, const Page& page);

        /**
         * @brief Allocate a new page at the end of the database file.
         *
         * Extends the file by @c kPageSize bytes and returns the ID of the
         * newly allocated page.  The page contents are undefined until written.
         *
         * @param[out] new_page_id Receives the ID of the allocated page.
         * @return @c Status::Ok on success, @c Status::IoError on failure.
         */
        [[nodiscard]] Status allocatePage(PageId& new_page_id);

        // ── Metadata ──────────────────────────────────────────────────────────────

        /// Return the total number of pages currently in the database file.
        [[nodiscard]] std::size_t pageCount() const;

        /// Return the filesystem path of the managed database file.
        [[nodiscard]] const std::filesystem::path& filePath() const;

        /// Force outstanding OS buffers to disk (fsync).
        [[nodiscard]] Status sync();

    private:
        std::filesystem::path path_; ///< Path to the open database file.
        int fd_;                     ///< POSIX file descriptor.
        std::size_t page_count_;     ///< Cached number of pages on disk.
    };

} // namespace hamdb
