#pragma once

/// @file buffer_pool_manager.hpp
/// @brief Manages a fixed-size pool of BufferFrames acting as a page cache.

#include "buffer/buffer_frame.hpp"
#include "buffer/lruk_replacer.hpp"
#include "storage/disk_manager.hpp"
#include <memory>
#include <unordered_map>

namespace hamdb
{

    /**
     * @brief The BufferPoolManager caches disk pages in memory.
     *
     * It uses a fixed-size array of BufferFrames and a hash map (page_table_)
     * to track which pages are currently in memory. When a page is requested,
     * it either returns the cached frame (hit) or loads it from disk into a
     * free frame (miss).
     *
     * This initial implementation does not feature an eviction policy.
     */
    class BufferPoolManager
    {
    public:
        // ── Construction ────────────────────────────────────────────────────────

        /// Initialize the buffer pool with a fixed size and a reference to the disk manager.
        BufferPoolManager(std::size_t pool_size, DiskManager& disk_manager);

        /// Flushes any dirty pages back to disk.
        ~BufferPoolManager();

        // Non-copyable, non-movable
        BufferPoolManager(const BufferPoolManager&) = delete;
        BufferPoolManager& operator=(const BufferPoolManager&) = delete;
        BufferPoolManager(BufferPoolManager&&) = delete;
        BufferPoolManager& operator=(BufferPoolManager&&) = delete;

        // ── Operations ──────────────────────────────────────────────────────────

        /**
         * @brief Fetches the requested page from the buffer pool.
         *
         * If the page is not in the pool, it will be read from disk into a free frame.
         * The returned frame will be pinned (pin_count incremented).
         *
         * @param page_id The logical ID of the page to fetch.
         * @param[out] out_frame A pointer to the pinned frame if successful.
         * @return Status::Ok on success, Status::BufferPoolFull if no free frame is available.
         */
        [[nodiscard]] Status fetchPage(PageId page_id, BufferFrame*& out_frame);

        /**
         * @brief Allocates a new page on disk and brings it into the buffer pool.
         *
         * The returned frame will be pinned.
         *
         * @param[out] out_page_id The logical ID of the newly allocated page.
         * @param[out] out_frame A pointer to the pinned frame containing the new page.
         * @return Status::Ok on success, Status::BufferPoolFull if no free frame is available.
         */
        [[nodiscard]] Status newPage(PageId& out_page_id, BufferFrame*& out_frame);

        /**
         * @brief Unpins a previously fetched page.
         *
         * @param page_id The logical ID of the page to unpin.
         * @param is_dirty If true, the page is marked as dirty and will be flushed later.
         * @return Status::Ok on success, Status::InvalidArg if the pin count drops below zero.
         */
        [[nodiscard]] Status unpinPage(PageId page_id, bool is_dirty);

        /**
         * @brief Flushes the specified page to disk if it is valid and dirty.
         *
         * @param page_id The logical ID of the page to flush.
         * @return Status::Ok on success or if page is clean, Status::InvalidArg if invalid.
         */
        [[nodiscard]] Status flushPage(PageId page_id);

        /**
         * @brief Flushes all valid, dirty pages in the buffer pool to disk.
         *
         * @return Status::Ok on success.
         */
        [[nodiscard]] Status flushAllPages();

    private:
        /**
         * @brief Finds an unused frame in the pool.
         *
         * For this milestone, a frame is free only if it is invalid (no eviction).
         *
         * @param[out] out_frame_id The ID of the free frame.
         * @return Status::Ok if found, Status::BufferPoolFull if not.
         */
        [[nodiscard]] Status findFreeFrame(FrameId& out_frame_id);

        std::size_t pool_size_;
        DiskManager& disk_manager_;
        
        // Fixed-size array of BufferFrames. Unique_ptr avoids vector resizing entirely.
        std::unique_ptr<BufferFrame[]> frames_;
        
        // Maps PageId to FrameId to quickly find cached pages.
        std::unordered_map<PageId, FrameId> page_table_;

        // Replacement policy manager.
        std::unique_ptr<LRUKReplacer> replacer_;
    };

} // namespace hamdb
