#pragma once

/// @file heap_iterator.hpp
/// @brief Iterator for sequential scans over a TableHeap.

#include "storage/disk_manager.hpp"
#include "storage/rid.hpp"
#include "storage/tuple.hpp"
#include <optional>

namespace hamdb
{

    class TableHeap; // forward declaration

    /**
     * @brief A forward iterator that scans all live tuples in a TableHeap.
     *
     * Iterates page-by-page, slot-by-slot, skipping deleted or invalid slots.
     */
    class HeapIterator
    {
    public:
        // ── Construction ──────────────────────────────────────────────────────────

        /// Default construct an end iterator.
        HeapIterator() = default;

        /**
         * @brief Construct an iterator pointing to the first valid tuple.
         * @param disk_manager Reference to the disk manager.
         * @param first_page_id The ID of the first page to scan.
         */
        HeapIterator(DiskManager& disk_manager, PageId first_page_id);

        // ── Iterator Operations ───────────────────────────────────────────────────

        [[nodiscard]] bool operator==(const HeapIterator& other) const noexcept;
        [[nodiscard]] bool operator!=(const HeapIterator& other) const noexcept;

        /**
         * @brief Advance to the next live tuple.
         * @return Reference to self.
         */
        HeapIterator& operator++();

        /**
         * @brief Dereference the iterator to retrieve the current tuple.
         * @return The current Tuple.
         */
        [[nodiscard]] Tuple operator*() const;

        /**
         * @brief Get the RID of the current tuple.
         */
        [[nodiscard]] RID getRID() const noexcept;

    private:
        void advanceToNextValid();
        void loadPage(PageId page_id);

        DiskManager* disk_manager_ = nullptr;
        PageId current_page_id_ = kInvalidPageId;
        std::uint16_t current_slot_id_ = 0;
        
        // Caching
        std::optional<Page> cached_page_;
    };

} // namespace hamdb
