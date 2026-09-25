#pragma once

/// @file table_heap.hpp
/// @brief A linked list of pages representing a database table.

#include "storage/disk_manager.hpp"
#include "storage/heap_iterator.hpp"
#include "storage/rid.hpp"
#include "storage/tuple.hpp"
#include <optional>

namespace hamdb
{

    /**
     * @brief Manages a collection of SlottedPages as a single table.
     *
     * @c TableHeap abstracts away individual pages. It handles inserts by finding
     * free space across pages (or allocating new ones), and coordinates page reads/writes
     * via @c DiskManager since there is no Buffer Pool yet.
     */
    class TableHeap
    {
    public:
        // ── Factory Methods ───────────────────────────────────────────────────────

        /**
         * @brief Create a new TableHeap, allocating its first page.
         * @param disk_manager The underlying disk manager.
         * @return A valid TableHeap on success, nullopt on I/O error.
         */
        static std::optional<TableHeap> create(DiskManager& disk_manager);

        /**
         * @brief Open an existing TableHeap.
         *
         * Scans the linked list of pages to rebuild internal metadata.
         *
         * @param disk_manager The underlying disk manager.
         * @param first_page_id The ID of the first page in the heap.
         * @return A valid TableHeap on success, nullopt on I/O error.
         */
        static std::optional<TableHeap> open(DiskManager& disk_manager, PageId first_page_id);

        // Move-only
        TableHeap(const TableHeap&) = delete;
        TableHeap& operator=(const TableHeap&) = delete;

        // TableHeap owns no movable resources because DiskManager is a reference.
        TableHeap(TableHeap&&) noexcept = default;
        TableHeap& operator=(TableHeap&&) noexcept = delete;

        // ── Tuple Operations ──────────────────────────────────────────────────────

        /**
         * @brief Insert a tuple into the heap.
         *
         * Will scan for free space starting from the first page, and allocate
         * a new page if necessary. Flushes modified pages to disk.
         *
         * @param tuple The tuple payload.
         * @param[out] rid The assigned RID on success.
         * @return Status::Ok on success, IO error otherwise.
         */
        [[nodiscard]] Status insertTuple(const Tuple& tuple, RID& rid);

        /**
         * @brief Read a tuple by its RID.
         * @param rid The location of the tuple.
         * @param[out] tuple Output tuple object.
         * @return Status::Ok, NotFound, or InvalidArg if deleted.
         */
        [[nodiscard]] Status readTuple(const RID& rid, Tuple& tuple) const;

        /**
         * @brief Delete a tuple by its RID.
         * @param rid The location of the tuple.
         * @return Status::Ok, NotFound, or InvalidArg if already deleted.
         */
        [[nodiscard]] Status deleteTuple(const RID& rid);

        // ── Metadata ──────────────────────────────────────────────────────────────

        [[nodiscard]] PageId getFirstPageId() const noexcept;
        [[nodiscard]] std::size_t getPageCount() const noexcept;
        [[nodiscard]] std::size_t getTupleCount() const noexcept;

        // ── Iteration ─────────────────────────────────────────────────────────────

        [[nodiscard]] HeapIterator begin() const;
        [[nodiscard]] HeapIterator end() const;

    private:
        TableHeap(DiskManager& disk_manager, PageId first_page_id, PageId last_page_id,
                  std::size_t page_count, std::size_t tuple_count);

        [[nodiscard]] Status bootstrapMetadata();

        DiskManager& disk_manager_;
        PageId first_page_id_ = kInvalidPageId;
        PageId last_page_id_ = kInvalidPageId;
        std::size_t page_count_ = 0;
        std::size_t tuple_count_ = 0;
    };

} // namespace hamdb
