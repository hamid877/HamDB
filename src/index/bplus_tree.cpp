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

            if (leaf.serialize(write_guard.pageMut().body()) != Status::Ok)
            {
                return Status::IoError;
            }
            write_guard.markDirty();
            write_guard.drop();

            if (sibling_leaf.serialize(sibling_guard.pageMut().body()) != Status::Ok)
            {
                return Status::IoError;
            }
            sibling_guard.markDirty();
            sibling_guard.drop();

            Status insert_status = insertIntoParent(leaf_page_id, sibling_leaf.keyAt(0), sibling_page_id);
            if (insert_status != Status::Ok)
            {
                return insert_status;
            }

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

    Status BPlusTree::insertIntoParent(PageId old_node_id, int64_t key, PageId new_node_id) noexcept
    {
        ReadPageGuard old_guard;
        if (bpm_.fetchPageRead(old_node_id, old_guard) != Status::Ok)
        {
            return Status::IoError;
        }

        BTreePage old_node_header;
        if (old_node_header.deserialize(old_guard.page().body()) != Status::Ok)
        {
            return Status::IoError;
        }

        PageId parent_page_id = old_node_header.parentPageId();
        old_guard.drop();

        if (parent_page_id == kInvalidPageId)
        {
            PageId new_root_page_id = kInvalidPageId;
            WritePageGuard new_root_guard;
            if (bpm_.newPageGuard(new_root_page_id, new_root_guard) != Status::Ok)
            {
                return Status::BufferPoolFull;
            }

            BTreeInternalPage new_root;
            new_root.init(new_root_page_id, kInvalidPageId);
            new_root.populateNewRoot(old_node_id, key, new_node_id);

            if (new_root.serialize(new_root_guard.pageMut().body()) != Status::Ok)
            {
                return Status::IoError;
            }
            new_root_guard.markDirty();

            WritePageGuard left_guard;
            if (bpm_.fetchPageWrite(old_node_id, left_guard) == Status::Ok)
            {
                BTreePage left_header;
                if (left_header.deserialize(left_guard.page().body()) == Status::Ok)
                {
                    left_header.setParentPageId(new_root_page_id);
                    if (left_header.serialize(left_guard.pageMut().body()) == Status::Ok)
                    {
                        left_guard.markDirty();
                    }
                }
            }

            WritePageGuard right_guard;
            if (bpm_.fetchPageWrite(new_node_id, right_guard) == Status::Ok)
            {
                BTreePage right_header;
                if (right_header.deserialize(right_guard.page().body()) == Status::Ok)
                {
                    right_header.setParentPageId(new_root_page_id);
                    if (right_header.serialize(right_guard.pageMut().body()) == Status::Ok)
                    {
                        right_guard.markDirty();
                    }
                }
            }

            root_page_id_ = new_root_page_id;
            return Status::Ok;
        }

        WritePageGuard parent_guard;
        if (bpm_.fetchPageWrite(parent_page_id, parent_guard) != Status::Ok)
        {
            return Status::IoError;
        }

        BTreeInternalPage parent;
        if (parent.deserialize(parent_guard.page().body()) != Status::Ok)
        {
            return Status::IoError;
        }

        if (!parent.isFull())
        {
            if (parent.insert(key, new_node_id) != Status::Ok)
            {
                return Status::IoError;
            }
            if (parent.serialize(parent_guard.pageMut().body()) != Status::Ok)
            {
                return Status::IoError;
            }
            parent_guard.markDirty();
            return Status::Ok;
        }

        PageId new_internal_page_id = kInvalidPageId;
        WritePageGuard new_internal_guard;
        if (bpm_.newPageGuard(new_internal_page_id, new_internal_guard) != Status::Ok)
        {
            return Status::BufferPoolFull;
        }

        BTreeInternalPage new_internal;
        new_internal.init(new_internal_page_id, parent.parentPageId());

        int64_t median_key = parent.moveHalfTo(new_internal, bpm_);

        Status insert_status = Status::Ok;
        if (key < median_key)
        {
            insert_status = parent.insert(key, new_node_id);
        }
        else
        {
            insert_status = new_internal.insert(key, new_node_id);
            if (insert_status == Status::Ok)
            {
                WritePageGuard child_guard;
                if (bpm_.fetchPageWrite(new_node_id, child_guard) == Status::Ok)
                {
                    BTreePage child_header;
                    if (child_header.deserialize(child_guard.page().body()) == Status::Ok)
                    {
                        child_header.setParentPageId(new_internal_page_id);
                        if (child_header.serialize(child_guard.pageMut().body()) == Status::Ok)
                        {
                            child_guard.markDirty();
                        }
                    }
                }
            }
        }

        if (insert_status != Status::Ok)
        {
            return insert_status;
        }

        if (parent.serialize(parent_guard.pageMut().body()) != Status::Ok)
        {
            return Status::IoError;
        }
        parent_guard.markDirty();

        if (new_internal.serialize(new_internal_guard.pageMut().body()) != Status::Ok)
        {
            return Status::IoError;
        }
        new_internal_guard.markDirty();

        parent_guard.drop();
        new_internal_guard.drop();

        return insertIntoParent(parent_page_id, median_key, new_internal_page_id);
    }

} // namespace hamdb
