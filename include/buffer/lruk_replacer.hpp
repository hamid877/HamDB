#pragma once

/// @file lruk_replacer.hpp
/// @brief LRU-K replacement policy implementation.

#include "buffer/buffer_frame.hpp"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace hamdb
{

    /**
     * @brief LRUKReplacer implements the LRU-K replacement policy.
     *
     * It tracks the access history of each frame. Frames with fewer than K
     * accesses are given +inf backward K-distance and are evicted before
     * frames with >= K accesses. Ties are broken using FIFO (oldest first),
     * and further by smaller FrameId.
     */
    class LRUKReplacer
    {
    public:
        // ── Construction ────────────────────────────────────────────────────────

        /**
         * @brief Construct a new LRUKReplacer.
         *
         * @param num_frames The maximum number of frames the replacer will track.
         * @param k The K value for the LRU-K policy.
         */
        LRUKReplacer(std::size_t num_frames, std::size_t k = 2);

        ~LRUKReplacer() = default;

        // Non-copyable, non-movable
        LRUKReplacer(const LRUKReplacer&) = delete;
        LRUKReplacer& operator=(const LRUKReplacer&) = delete;
        LRUKReplacer(LRUKReplacer&&) = delete;
        LRUKReplacer& operator=(LRUKReplacer&&) = delete;

        // ── Operations ──────────────────────────────────────────────────────────

        /**
         * @brief Record an access event for a specific frame.
         *
         * @param frame_id The ID of the accessed frame.
         */
        void recordAccess(FrameId frame_id);

        /**
         * @brief Update whether a frame can be evicted.
         *
         * @param frame_id The ID of the frame.
         * @param set_evictable True if the frame should be considered for eviction.
         */
        void setEvictable(FrameId frame_id, bool set_evictable);

        /**
         * @brief Find the best victim frame to evict.
         *
         * @param[out] out_frame_id The ID of the chosen victim frame.
         * @return true if a victim was found, false if no frames are evictable.
         */
        bool evict(FrameId& out_frame_id);

        /**
         * @brief Remove a frame from the replacer's tracking metadata.
         *
         * @param frame_id The ID of the frame to remove.
         * @throws std::invalid_argument if the frame is pinned (non-evictable).
         */
        void remove(FrameId frame_id);

        /**
         * @brief Get the number of currently evictable frames.
         *
         * @return std::size_t The count of evictable frames.
         */
        [[nodiscard]] std::size_t size() const;

    private:
        struct LRUKNode {
            std::deque<std::uint64_t> history_;
            bool is_evictable_{false};
        };

        std::size_t k_;
        std::size_t num_frames_;
        std::size_t curr_size_{0};
        std::uint64_t current_timestamp_{0};
        std::vector<LRUKNode> node_store_;
    };

} // namespace hamdb
