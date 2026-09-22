#include "storage/heap_iterator.hpp"
#include "storage/slotted_page.hpp"

namespace hamdb
{

    HeapIterator::HeapIterator(DiskManager& disk_manager, PageId first_page_id)
        : disk_manager_(&disk_manager), current_page_id_(first_page_id)
    {
        if (current_page_id_ != kInvalidPageId)
        {
            loadPage(current_page_id_);
            advanceToNextValid();
        }
    }

    bool HeapIterator::operator==(const HeapIterator& other) const noexcept
    {
        return current_page_id_ == other.current_page_id_ &&
               current_slot_id_ == other.current_slot_id_;
    }

    bool HeapIterator::operator!=(const HeapIterator& other) const noexcept
    {
        return !(*this == other);
    }

    HeapIterator& HeapIterator::operator++()
    {
        if (current_page_id_ == kInvalidPageId)
        {
            return *this;
        }

        current_slot_id_++;
        advanceToNextValid();
        return *this;
    }

    Tuple HeapIterator::operator*() const
    {
        if (!cached_page_.has_value())
        {
            return {};
        }
        
        SlottedPage sp(const_cast<Page&>(cached_page_.value()));
        Tuple tuple;
        if (sp.readTuple(current_slot_id_, tuple) == Status::Ok)
        {
            return tuple;
        }
        return {};
    }

    RID HeapIterator::getRID() const noexcept
    {
        return RID(current_page_id_, current_slot_id_);
    }

    void HeapIterator::advanceToNextValid()
    {
        while (current_page_id_ != kInvalidPageId && cached_page_.has_value())
        {
            SlottedPage sp(cached_page_.value());
            
            while (current_slot_id_ < sp.slotCount())
            {
                Tuple tmp;
                if (sp.readTuple(current_slot_id_, tmp) == Status::Ok)
                {
                    // Found a valid tuple
                    return;
                }
                // Skip deleted slots
                current_slot_id_++;
            }

            // Exhausted current page, move to next
            current_page_id_ = sp.getNextPageId();
            current_slot_id_ = 0;
            
            if (current_page_id_ != kInvalidPageId)
            {
                loadPage(current_page_id_);
            }
            else
            {
                cached_page_ = std::nullopt;
            }
        }
    }

    void HeapIterator::loadPage(PageId page_id)
    {
        Page page;
        if (disk_manager_->readPage(page_id, page) == Status::Ok)
        {
            cached_page_ = std::move(page);
        }
        else
        {
            cached_page_ = std::nullopt;
            current_page_id_ = kInvalidPageId;
        }
    }

} // namespace hamdb
