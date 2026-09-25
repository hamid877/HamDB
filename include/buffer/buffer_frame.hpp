#pragma once

/// @file buffer_frame.hpp
/// @brief A wrapper around a Page that tracks its usage in the Buffer Pool.

#include "common/constants.hpp"
#include "storage/page.hpp"
#include <cstdint>

namespace hamdb
{

    /// Identifier for a frame within the buffer pool array.
    using FrameId = std::uint32_t;

    /**
     * @brief Encapsulates a Page and its metadata for the Buffer Pool Manager.
     *
     * A buffer frame physically owns a Page object. It tracks whether the page
     * is valid (contains actual data from disk), how many threads currently hold
     * a pin on the page, whether it has been modified (dirty), and its logical ID.
     */
    class BufferFrame
    {
    public:
        // ── Construction ────────────────────────────────────────────────────────

        /// Construct an invalid frame.
        BufferFrame();

        // Frames hold Pages (which are non-copyable), so frames are non-copyable.
        BufferFrame(const BufferFrame&) = delete;
        BufferFrame& operator=(const BufferFrame&) = delete;

        BufferFrame(BufferFrame&&) = default;
        BufferFrame& operator=(BufferFrame&&) = default;

        // ── Accessors ───────────────────────────────────────────────────────────

        [[nodiscard]] FrameId frameId() const;
        void setFrameId(FrameId frame_id);

        [[nodiscard]] PageId pageId() const;
        void setPageId(PageId page_id);

        [[nodiscard]] Page& page();
        [[nodiscard]] const Page& page() const;

        [[nodiscard]] int pinCount() const;
        [[nodiscard]] bool isDirty() const;
        [[nodiscard]] bool isValid() const;

        // ── Mutators ────────────────────────────────────────────────────────────

        /// Increment the pin count.
        void pin();

        /// Decrement the pin count. If @p dirty is true, marks the frame as dirty.
        /// Does nothing to the pin count if it's already 0.
        void unpin(bool dirty);

        /// Mark the frame as clean (e.g. after flushing).
        void markClean();

        /// Invalidate the frame (e.g. after eviction or on creation).
        void invalidate();

        /// Reset the frame for a new page. Sets pin count to 1, dirty to false, valid to true.
        void reset(PageId new_page_id);

    private:
        Page page_;                      ///< The actual page data.
        PageId page_id_{kInvalidPageId}; ///< The logical ID of the page.
        FrameId frame_id_{0};            ///< The index of this frame in the pool.
        int pin_count_{0};               ///< Number of active users of this page.
        bool is_dirty_{false};           ///< True if the page has been modified.
        bool is_valid_{false};           ///< True if the frame holds a valid page.
    };

} // namespace hamdb
