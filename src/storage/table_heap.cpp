#include "storage/table_heap.hpp"
#include "storage/slotted_page.hpp"

namespace hamdb
{

    TableHeap::TableHeap(DiskManager& disk_manager, PageId first_page_id, PageId last_page_id,
                         std::size_t page_count, std::size_t tuple_count)
        : disk_manager_(disk_manager),
          first_page_id_(first_page_id),
          last_page_id_(last_page_id),
          page_count_(page_count),
          tuple_count_(tuple_count)
    {
    }

    std::optional<TableHeap> TableHeap::create(DiskManager& disk_manager)
    {
        PageId first_page_id = kInvalidPageId;
        if (disk_manager.allocatePage(first_page_id) != Status::Ok)
        {
            return std::nullopt;
        }

        Page page(PageHeader(first_page_id, PageType::Table));
        SlottedPage sp(page);
        (void)sp.initialize();

        if (disk_manager.writePage(first_page_id, page) != Status::Ok)
        {
            return std::nullopt;
        }

        return TableHeap(disk_manager, first_page_id, first_page_id, 1, 0);
    }

    std::optional<TableHeap> TableHeap::open(DiskManager& disk_manager, PageId first_page_id)
    {
        TableHeap heap(disk_manager, first_page_id, first_page_id, 0, 0);
        if (heap.bootstrapMetadata() != Status::Ok)
        {
            return std::nullopt;
        }
        return heap;
    }

    Status TableHeap::bootstrapMetadata()
    {
        PageId current_id = first_page_id_;
        PageId last_id = kInvalidPageId;
        std::size_t pages = 0;
        std::size_t tuples = 0;

        while (current_id != kInvalidPageId)
        {
            Page page;
            if (disk_manager_.readPage(current_id, page) != Status::Ok)
            {
                return Status::IoError;
            }

            SlottedPage sp(page);
            tuples += sp.tupleCount();
            pages++;
            last_id = current_id;
            current_id = sp.getNextPageId();
        }

        last_page_id_ = last_id;
        page_count_ = pages;
        tuple_count_ = tuples;

        return Status::Ok;
    }

    Status TableHeap::insertTuple(const Tuple& tuple, RID& rid)
    {
        if (tuple.empty())
        {
            return Status::InvalidArg;
        }

        PageId current_id = first_page_id_;
        Page page;

        while (current_id != kInvalidPageId)
        {
            if (disk_manager_.readPage(current_id, page) != Status::Ok)
            {
                return Status::IoError;
            }

            SlottedPage sp(page);
            if (!sp.isFull(tuple.size()))
            {
                SlotId slot_id = kInvalidSlotId;
                if (sp.insertTuple(tuple, slot_id) == Status::Ok)
                {
                    // Mark page dirty and write back
                    if (disk_manager_.writePage(current_id, page) != Status::Ok)
                    {
                        return Status::IoError;
                    }

                    rid = RID(current_id, slot_id);
                    tuple_count_++;
                    return Status::Ok;
                }
            }

            // Move to next page
            PageId next_id = sp.getNextPageId();
            
            // If there's no next page, we must break and allocate one.
            if (next_id == kInvalidPageId)
            {
                break;
            }
            current_id = next_id;
        }

        // Need to allocate a new page
        PageId new_page_id = kInvalidPageId;
        if (disk_manager_.allocatePage(new_page_id) != Status::Ok)
        {
            return Status::IoError;
        }

        // Update old last page links
        if (current_id != kInvalidPageId)
        {
            // page currently holds the last page
            SlottedPage old_sp(page);
            old_sp.setNextPageId(new_page_id);
            if (disk_manager_.writePage(current_id, page) != Status::Ok)
            {
                return Status::IoError;
            }
        }

        // Initialize new page
        Page new_page(PageHeader(new_page_id, PageType::Table));
        SlottedPage new_sp(new_page);
        (void)new_sp.initialize();
        new_sp.setPrevPageId(current_id);

        SlotId slot_id = kInvalidSlotId;
        if (new_sp.insertTuple(tuple, slot_id) != Status::Ok)
        {
            // Tuple too large for an empty page
            return Status::IoError; 
        }

        if (disk_manager_.writePage(new_page_id, new_page) != Status::Ok)
        {
            return Status::IoError;
        }

        last_page_id_ = new_page_id;
        page_count_++;
        tuple_count_++;
        rid = RID(new_page_id, slot_id);

        return Status::Ok;
    }

    Status TableHeap::readTuple(const RID& rid, Tuple& tuple) const
    {
        if (!rid.isValid())
        {
            return Status::InvalidArg;
        }

        Page page;
        if (disk_manager_.readPage(rid.getPageId(), page) != Status::Ok)
        {
            return Status::NotFound;
        }

        SlottedPage sp(page);
        return sp.readTuple(rid.getSlotId(), tuple);
    }

    Status TableHeap::deleteTuple(const RID& rid)
    {
        if (!rid.isValid())
        {
            return Status::InvalidArg;
        }

        Page page;
        if (disk_manager_.readPage(rid.getPageId(), page) != Status::Ok)
        {
            return Status::NotFound;
        }

        SlottedPage sp(page);
        Status s = sp.deleteTuple(rid.getSlotId());
        if (s == Status::Ok)
        {
            if (disk_manager_.writePage(rid.getPageId(), page) != Status::Ok)
            {
                return Status::IoError;
            }
            tuple_count_--;
        }
        return s;
    }

    PageId TableHeap::getFirstPageId() const noexcept
    {
        return first_page_id_;
    }

    std::size_t TableHeap::getPageCount() const noexcept
    {
        return page_count_;
    }

    std::size_t TableHeap::getTupleCount() const noexcept
    {
        return tuple_count_;
    }

    HeapIterator TableHeap::begin() const
    {
        return HeapIterator(disk_manager_, first_page_id_);
    }

    HeapIterator TableHeap::end() const
    {
        return {};
    }

} // namespace hamdb
