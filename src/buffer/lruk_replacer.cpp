#include "buffer/lruk_replacer.hpp"
#include <stdexcept>

namespace hamdb
{

    LRUKReplacer::LRUKReplacer(std::size_t num_frames, std::size_t k)
        : k_(k), num_frames_(num_frames), node_store_(num_frames)
    {
    }

    void LRUKReplacer::recordAccess(FrameId frame_id)
    {
        if (frame_id >= num_frames_)
        {
            throw std::invalid_argument("Invalid frame_id");
        }

        auto& node = node_store_[frame_id];
        current_timestamp_++;
        node.history_.push_back(current_timestamp_);

        if (node.history_.size() > k_)
        {
            node.history_.pop_front();
        }
    }

    void LRUKReplacer::setEvictable(FrameId frame_id, bool set_evictable)
    {
        if (frame_id >= num_frames_)
        {
            throw std::invalid_argument("Invalid frame_id");
        }

        auto& node = node_store_[frame_id];
        // Only valid to set evictable if it has access history
        if (node.history_.empty())
        {
            return;
        }

        if (node.is_evictable_ != set_evictable)
        {
            node.is_evictable_ = set_evictable;
            if (set_evictable)
            {
                curr_size_++;
            }
            else
            {
                curr_size_--;
            }
        }
    }

    bool LRUKReplacer::evict(FrameId& out_frame_id)
    {
        bool found_victim = false;
        FrameId best_fid = 0;
        bool best_is_inf = false;
        std::uint64_t best_timestamp = 0;

        for (std::size_t i = 0; i < num_frames_; ++i)
        {
            const auto& node = node_store_[i];
            if (!node.is_evictable_ || node.history_.empty())
            {
                continue;
            }

            bool is_inf = (node.history_.size() < k_);
            std::uint64_t earliest_timestamp = node.history_.front();

            if (!found_victim)
            {
                found_victim = true;
                best_fid = static_cast<FrameId>(i);
                best_is_inf = is_inf;
                best_timestamp = earliest_timestamp;
                continue;
            }

            if (is_inf && !best_is_inf)
            {
                // +inf distance always wins against finite distance
                best_fid = static_cast<FrameId>(i);
                best_is_inf = is_inf;
                best_timestamp = earliest_timestamp;
            }
            else if (is_inf == best_is_inf)
            {
                // Tie breaker 1: smallest earliest_timestamp wins
                if (earliest_timestamp < best_timestamp)
                {
                    best_fid = static_cast<FrameId>(i);
                    best_timestamp = earliest_timestamp;
                }
                else if (earliest_timestamp == best_timestamp)
                {
                    // Tie breaker 2: smaller FrameId wins
                    if (static_cast<FrameId>(i) < best_fid)
                    {
                        best_fid = static_cast<FrameId>(i);
                    }
                }
            }
        }

        if (found_victim)
        {
            out_frame_id = best_fid;
            // Note: We do NOT remove metadata here. The caller will do it via remove().
            return true;
        }

        return false;
    }

    void LRUKReplacer::remove(FrameId frame_id)
    {
        if (frame_id >= num_frames_)
        {
            throw std::invalid_argument("Invalid frame_id");
        }

        auto& node = node_store_[frame_id];
        if (node.history_.empty())
        {
            return;
        }

        if (!node.is_evictable_)
        {
            throw std::invalid_argument("Cannot remove non-evictable frame");
        }

        node.history_.clear();
        node.is_evictable_ = false;
        curr_size_--;
    }

    std::size_t LRUKReplacer::size() const
    {
        return curr_size_;
    }

} // namespace hamdb
