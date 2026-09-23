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

            std::ranges::fill(guard.pageMut().body(), std::byte{0});

            BTreeLeafPage leaf;
            leaf.init(new_page_id, kInvalidPageId);
            Status status = leaf.insert(key, rid);
            
            if (leaf.serialize(guard.pageMut().body()) != Status::Ok)
            {
                return Status::IoError;
            }
            
            root_page_id_ = new_page_id;
            guard.markDirty();
            
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

        if (leaf.lookup(key).has_value())
        {
            return Status::AlreadyExists;
        }

        if (leaf.isFull())
        {
            PageId sibling_page_id = kInvalidPageId;
            WritePageGuard sibling_guard;
            if (bpm_.newPageGuard(sibling_page_id, sibling_guard) != Status::Ok)
            {
                return Status::BufferPoolFull;
            }

            BTreeLeafPage sibling_leaf;
            sibling_leaf.init(sibling_page_id, leaf.parentPageId());

            leaf.moveHalfTo(sibling_leaf);

            sibling_leaf.setNextPageId(leaf.nextPageId());
            sibling_leaf.setPrevPageId(leaf_page_id);
            leaf.setNextPageId(sibling_page_id);

            if (sibling_leaf.nextPageId() != kInvalidPageId)
            {
                WritePageGuard right_guard;
                if (bpm_.fetchPageWrite(sibling_leaf.nextPageId(), right_guard) == Status::Ok)
                {
                    BTreeLeafPage right_sibling;
                    if (right_sibling.deserialize(right_guard.page().body()) == Status::Ok)
                    {
                        right_sibling.setPrevPageId(sibling_page_id);
                        if (right_sibling.serialize(right_guard.pageMut().body()) == Status::Ok)
                        {
                            right_guard.markDirty();
                        }
                    }
                }
            }

            Status status = Status::Ok;
            if (key < sibling_leaf.keyAt(0))
            {
                status = leaf.insert(key, rid);
            }
            else
            {
                status = sibling_leaf.insert(key, rid);
            }

            if (status != Status::Ok)
            {
                return status;
            }

            if (leaf.parentPageId() == kInvalidPageId)
            {
                PageId new_root_page_id = kInvalidPageId;
                WritePageGuard new_root_guard;
                if (bpm_.newPageGuard(new_root_page_id, new_root_guard) != Status::Ok)
                {
                    return Status::BufferPoolFull;
                }

                BTreeInternalPage new_root;
                new_root.init(new_root_page_id, kInvalidPageId);
                new_root.populateNewRoot(leaf_page_id, sibling_leaf.keyAt(0), sibling_page_id);

                if (new_root.serialize(new_root_guard.pageMut().body()) != Status::Ok)
                {
                    return Status::IoError;
                }
                new_root_guard.markDirty();

                leaf.setParentPageId(new_root_page_id);
                sibling_leaf.setParentPageId(new_root_page_id);
                root_page_id_ = new_root_page_id;
            }
            else
            {
                WritePageGuard parent_guard;
                if (bpm_.fetchPageWrite(leaf.parentPageId(), parent_guard) != Status::Ok)
                {
                    return Status::IoError;
                }

                BTreeInternalPage parent;
                if (parent.deserialize(parent_guard.page().body()) != Status::Ok)
                {
                    return Status::IoError;
                }

                if (parent.isFull())
                {
                    return Status::PageFull;
                }

                if (parent.insert(sibling_leaf.keyAt(0), sibling_page_id) != Status::Ok)
                {
                    return Status::IoError;
                }

                if (parent.serialize(parent_guard.pageMut().body()) != Status::Ok)
                {
                    return Status::IoError;
                }
                parent_guard.markDirty();
            }

            if (leaf.serialize(write_guard.pageMut().body()) != Status::Ok)
            {
                return Status::IoError;
            }
            write_guard.markDirty();

            if (sibling_leaf.serialize(sibling_guard.pageMut().body()) != Status::Ok)
            {
                return Status::IoError;
            }
            sibling_guard.markDirty();

            return Status::Ok;
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

        return status;
    }

} // namespace hamdb
