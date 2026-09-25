#include "index/bplus_tree_iterator.hpp"

namespace hamdb
{

    BPlusTreeIterator::BPlusTreeIterator(BufferPoolManager& bpm, PageId page_id,
                                         uint16_t index) noexcept
        : bpm_(&bpm), page_id_(page_id), index_(index)
    {
        if (page_id_ != kInvalidPageId)
        {
            loadPage();
        }
    }

    BPlusTreeIterator::BPlusTreeIterator(BPlusTreeIterator&& other) noexcept
        : bpm_(other.bpm_), page_id_(other.page_id_), index_(other.index_),
          guard_(std::move(other.guard_)), leaf_(std::move(other.leaf_))
    {
        other.bpm_ = nullptr;
        other.page_id_ = kInvalidPageId;
        other.index_ = 0;
    }

    BPlusTreeIterator& BPlusTreeIterator::operator=(BPlusTreeIterator&& other) noexcept
    {
        if (this != &other)
        {
            bpm_ = other.bpm_;
            page_id_ = other.page_id_;
            index_ = other.index_;
            guard_ = std::move(other.guard_);
            leaf_ = std::move(other.leaf_);

            other.bpm_ = nullptr;
            other.page_id_ = kInvalidPageId;
            other.index_ = 0;
        }
        return *this;
    }

    bool BPlusTreeIterator::isEnd() const noexcept
    {
        return page_id_ == kInvalidPageId;
    }

    std::pair<int64_t, RID> BPlusTreeIterator::operator*() const noexcept
    {
        if (isEnd())
        {
            return {0, RID{}}; // Should ideally not be called when isEnd() is true
        }
        return {leaf_.keyAt(index_), leaf_.valueAt(index_)};
    }

    BPlusTreeIterator& BPlusTreeIterator::operator++() noexcept
    {
        if (isEnd())
        {
            return *this;
        }

        if (index_ + 1 < leaf_.size())
        {
            index_++;
        }
        else
        {
            PageId next_page_id = leaf_.nextPageId();
            guard_.reset(); // Drop the guard to avoid pin leaks
            page_id_ = next_page_id;
            index_ = 0;

            if (page_id_ != kInvalidPageId)
            {
                loadPage();
            }
        }

        return *this;
    }

    bool BPlusTreeIterator::operator==(const BPlusTreeIterator& other) const noexcept
    {
        // Treat two end iterators as equal
        if (isEnd() && other.isEnd())
        {
            return true;
        }
        return page_id_ == other.page_id_ && index_ == other.index_;
    }

    bool BPlusTreeIterator::operator!=(const BPlusTreeIterator& other) const noexcept
    {
        return !(*this == other);
    }

    void BPlusTreeIterator::loadPage() noexcept
    {
        guard_.emplace();
        if (bpm_->fetchPageRead(page_id_, guard_.value()) == Status::Ok)
        {
            if (leaf_.deserialize(guard_.value().page().body()) != Status::Ok)
            {
                // In case of corruption or failure, turn this into an end iterator
                guard_.reset();
                page_id_ = kInvalidPageId;
                index_ = 0;
            }
        }
        else
        {
            guard_.reset();
            page_id_ = kInvalidPageId;
            index_ = 0;
        }
    }

} // namespace hamdb
