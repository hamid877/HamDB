#include "index/bplus_tree.hpp"
#include "index/btree_internal_page.hpp"
#include "index/btree_leaf_page.hpp"
#include "index/btree_page.hpp"

namespace hamdb
{

    BPlusTree::BPlusTree(BufferPoolManager& bpm) noexcept
        : bpm_(bpm)
    {
    }

    void BPlusTree::create() noexcept
    {
        root_page_id_ = kInvalidPageId;
    }

    void BPlusTree::open(PageId root_page_id) noexcept
    {
        root_page_id_ = root_page_id;
    }

    bool BPlusTree::isEmpty() const noexcept
    {
        return root_page_id_ == kInvalidPageId;
    }

    std::optional<RID> BPlusTree::getValue(int64_t key) noexcept
    {
        if (isEmpty())
        {
            return std::nullopt;
        }

        PageId curr_page_id = root_page_id_;

        while (curr_page_id != kInvalidPageId)
        {
            ReadPageGuard guard;
            if (bpm_.fetchPageRead(curr_page_id, guard) != Status::Ok)
            {
                return std::nullopt;
            }

            const auto& page = guard.page();

            BTreePage header;
            if (header.deserialize(page.body()) != Status::Ok)
            {
                return std::nullopt;
            }

            if (header.pageType() == PageType::BTreeLeaf)
            {
                BTreeLeafPage leaf;
                if (leaf.deserialize(page.body()) != Status::Ok)
                {
                    return std::nullopt;
                }
                return leaf.lookup(key);
            }
            else if (header.pageType() == PageType::BTreeInternal)
            {
                BTreeInternalPage internal;
                if (internal.deserialize(page.body()) != Status::Ok)
                {
                    return std::nullopt;
                }
                
                PageId next_page_id = internal.lookup(key);
                if (next_page_id == kInvalidPageId)
                {
                    return std::nullopt;
                }
                curr_page_id = next_page_id;
            }
            else
            {
                return std::nullopt;
            }
        }

        return std::nullopt;
    }

    Status BPlusTree::insert(int64_t key, RID rid) noexcept
    {
        if (isEmpty())
        {
            PageId new_page_id = kInvalidPageId;
            WritePageGuard guard;
            if (bpm_.newPageGuard(new_page_id, guard) != Status::Ok)
            {
                return Status::BufferPoolFull;
            }

            root_page_id_ = new_page_id;

            BTreeLeafPage leaf;
            leaf.init(new_page_id, kInvalidPageId);
            Status status = leaf.insert(key, rid);
            if (status == Status::Ok)
            {
                if (leaf.serialize(guard.pageMut().body()) != Status::Ok)
                {
                    return Status::IoError;
                }
                guard.markDirty();
            }
            return status;
        }

        PageId curr_page_id = root_page_id_;
        PageId leaf_page_id = kInvalidPageId;

        while (curr_page_id != kInvalidPageId)
        {
            ReadPageGuard guard;
            if (bpm_.fetchPageRead(curr_page_id, guard) != Status::Ok)
            {
                return Status::IoError;
            }

            const auto& page = guard.page();

            BTreePage header;
            if (header.deserialize(page.body()) != Status::Ok)
            {
                return Status::IoError;
            }

            if (header.pageType() == PageType::BTreeLeaf)
            {
                leaf_page_id = curr_page_id;
                break;
            }
            else if (header.pageType() == PageType::BTreeInternal)
            {
                BTreeInternalPage internal;
                if (internal.deserialize(page.body()) != Status::Ok)
                {
                    return Status::IoError;
                }
                curr_page_id = internal.lookup(key);
            }
            else
            {
                return Status::Corruption;
            }
        }

        if (leaf_page_id == kInvalidPageId)
        {
            return Status::NotFound;
        }

        WritePageGuard write_guard;
        if (bpm_.fetchPageWrite(leaf_page_id, write_guard) != Status::Ok)
        {
            return Status::IoError;
        }

        BTreeLeafPage leaf;
        if (leaf.deserialize(write_guard.page().body()) != Status::Ok)
        {
            return Status::IoError;
        }

        if (leaf.isFull())
        {
            return Status::PageFull;
        }

        Status status = leaf.insert(key, rid);
        if (status == Status::Ok)
        {
            if (leaf.serialize(write_guard.pageMut().body()) != Status::Ok)
            {
                return Status::IoError;
            }
            write_guard.markDirty();
        }
        else if (status == Status::InvalidArg)
        {
            return Status::PageFull;
        }

        return status;
    }

} // namespace hamdb
