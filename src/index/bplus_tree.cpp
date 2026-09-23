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

        int64_t median_key = parent.moveHalfTo(new_internal);

        for (uint16_t i = 0; i < new_internal.size(); ++i)
        {
            PageId child_page_id = new_internal.childAt(i);
            WritePageGuard child_guard;
            if (bpm_.fetchPageWrite(child_page_id, child_guard) == Status::Ok)
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

    Status BPlusTree::remove(int64_t key) noexcept
    {
        if (isEmpty())
        {
            return Status::NotFound;
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

        Status status = leaf.remove(key);
        if (status != Status::Ok)
        {
            return status;
        }

        if (leaf.serialize(write_guard.pageMut().body()) != Status::Ok)
        {
            return Status::IoError;
        }
        write_guard.markDirty();
        
        bool underflow = leaf.size() < leaf.minSize();
        bool is_empty_root = leaf_page_id == root_page_id_ && leaf.isEmpty();
        
        write_guard.drop();

        if (is_empty_root)
        {
            root_page_id_ = kInvalidPageId;
            return Status::Ok;
        }

        if (underflow && leaf_page_id != root_page_id_)
        {
            return handleUnderflow(leaf_page_id);
        }

        return Status::Ok;
    }

    Status BPlusTree::handleUnderflow(PageId page_id) noexcept
    {
        if (page_id == root_page_id_)
        {
            WritePageGuard root_guard;
            if (bpm_.fetchPageWrite(root_page_id_, root_guard) != Status::Ok)
            {
                return Status::IoError;
            }
            BTreePage root_header;
            if (root_header.deserialize(root_guard.page().body()) != Status::Ok)
            {
                return Status::IoError;
            }

            if (root_header.pageType() == PageType::BTreeInternal && root_header.currentSize() == 1)
            {
                BTreeInternalPage root_internal;
                if (root_internal.deserialize(root_guard.page().body()) != Status::Ok)
                {
                    return Status::IoError;
                }
                PageId new_root_id = root_internal.childAt(0);

                WritePageGuard child_guard;
                if (bpm_.fetchPageWrite(new_root_id, child_guard) == Status::Ok)
                {
                    BTreePage child_header;
                    if (child_header.deserialize(child_guard.page().body()) == Status::Ok)
                    {
                        child_header.setParentPageId(kInvalidPageId);
                        if (child_header.serialize(child_guard.pageMut().body()) == Status::Ok)
                        {
                            child_guard.markDirty();
                        }
                    }
                }
                root_page_id_ = new_root_id;
            }
            return Status::Ok;
        }

        WritePageGuard node_guard;
        if (bpm_.fetchPageWrite(page_id, node_guard) != Status::Ok)
        {
            return Status::IoError;
        }
        BTreePage node_header;
        if (node_header.deserialize(node_guard.page().body()) != Status::Ok)
        {
            return Status::IoError;
        }

        PageId parent_id = node_header.parentPageId();
        WritePageGuard parent_guard;
        if (bpm_.fetchPageWrite(parent_id, parent_guard) != Status::Ok)
        {
            return Status::IoError;
        }
        BTreeInternalPage parent;
        if (parent.deserialize(parent_guard.page().body()) != Status::Ok)
        {
            return Status::IoError;
        }

        int child_idx = parent.findChildIndex(page_id);
        if (child_idx == -1)
        {
            return Status::Corruption;
        }

        PageId left_sibling_id = child_idx > 0 ? parent.childAt(child_idx - 1) : kInvalidPageId;
        PageId right_sibling_id = child_idx < parent.size() - 1 ? parent.childAt(child_idx + 1) : kInvalidPageId;

        // Try to borrow from left sibling
        if (left_sibling_id != kInvalidPageId)
        {
            WritePageGuard sibling_guard;
            if (bpm_.fetchPageWrite(left_sibling_id, sibling_guard) == Status::Ok)
            {
                BTreePage sibling_header;
                if (sibling_header.deserialize(sibling_guard.page().body()) == Status::Ok)
                {
                    if (sibling_header.currentSize() > sibling_header.minSize())
                    {
                        if (node_header.pageType() == PageType::BTreeLeaf)
                        {
                            BTreeLeafPage node, sibling;
                            if (node.deserialize(node_guard.page().body()) != Status::Ok) return Status::IoError;
                            if (sibling.deserialize(sibling_guard.page().body()) != Status::Ok) return Status::IoError;

                            sibling.moveLastToFrontOf(node);
                            parent.setKeyAt(child_idx, node.keyAt(0));

                            if (node.serialize(node_guard.pageMut().body()) != Status::Ok) return Status::IoError;
                            if (sibling.serialize(sibling_guard.pageMut().body()) != Status::Ok) return Status::IoError;
                        }
                        else
                        {
                            BTreeInternalPage node, sibling;
                            if (node.deserialize(node_guard.page().body()) != Status::Ok) return Status::IoError;
                            if (sibling.deserialize(sibling_guard.page().body()) != Status::Ok) return Status::IoError;

                            int64_t new_middle = sibling.moveLastToFrontOf(node, parent.keyAt(child_idx));
                            parent.setKeyAt(child_idx, new_middle);

                            PageId moved_child = node.childAt(0);
                            WritePageGuard moved_guard;
                            if (bpm_.fetchPageWrite(moved_child, moved_guard) == Status::Ok)
                            {
                                BTreePage moved_header;
                                if (moved_header.deserialize(moved_guard.page().body()) == Status::Ok)
                                {
                                    moved_header.setParentPageId(page_id);
                                    if (moved_header.serialize(moved_guard.pageMut().body()) != Status::Ok) return Status::IoError;
                                    moved_guard.markDirty();
                                }
                            }

                            if (node.serialize(node_guard.pageMut().body()) != Status::Ok) return Status::IoError;
                            if (sibling.serialize(sibling_guard.pageMut().body()) != Status::Ok) return Status::IoError;
                        }
                        if (parent.serialize(parent_guard.pageMut().body()) != Status::Ok) return Status::IoError;
                        node_guard.markDirty();
                        sibling_guard.markDirty();
                        parent_guard.markDirty();
                        return Status::Ok;
                    }
                }
            }
        }

        // Try to borrow from right sibling
        if (right_sibling_id != kInvalidPageId)
        {
            WritePageGuard sibling_guard;
            if (bpm_.fetchPageWrite(right_sibling_id, sibling_guard) == Status::Ok)
            {
                BTreePage sibling_header;
                if (sibling_header.deserialize(sibling_guard.page().body()) == Status::Ok)
                {
                    if (sibling_header.currentSize() > sibling_header.minSize())
                    {
                        if (node_header.pageType() == PageType::BTreeLeaf)
                        {
                            BTreeLeafPage node, sibling;
                            if (node.deserialize(node_guard.page().body()) != Status::Ok) return Status::IoError;
                            if (sibling.deserialize(sibling_guard.page().body()) != Status::Ok) return Status::IoError;

                            sibling.moveFirstToEndOf(node);
                            parent.setKeyAt(child_idx + 1, sibling.keyAt(0));

                            if (node.serialize(node_guard.pageMut().body()) != Status::Ok) return Status::IoError;
                            if (sibling.serialize(sibling_guard.pageMut().body()) != Status::Ok) return Status::IoError;
                        }
                        else
                        {
                            BTreeInternalPage node, sibling;
                            if (node.deserialize(node_guard.page().body()) != Status::Ok) return Status::IoError;
                            if (sibling.deserialize(sibling_guard.page().body()) != Status::Ok) return Status::IoError;

                            int64_t new_middle = sibling.moveFirstToEndOf(node, parent.keyAt(child_idx + 1));
                            parent.setKeyAt(child_idx + 1, new_middle);

                            PageId moved_child = node.childAt(node.size() - 1);
                            WritePageGuard moved_guard;
                            if (bpm_.fetchPageWrite(moved_child, moved_guard) == Status::Ok)
                            {
                                BTreePage moved_header;
                                if (moved_header.deserialize(moved_guard.page().body()) == Status::Ok)
                                {
                                    moved_header.setParentPageId(page_id);
                                    if (moved_header.serialize(moved_guard.pageMut().body()) != Status::Ok) return Status::IoError;
                                    moved_guard.markDirty();
                                }
                            }

                            if (node.serialize(node_guard.pageMut().body()) != Status::Ok) return Status::IoError;
                            if (sibling.serialize(sibling_guard.pageMut().body()) != Status::Ok) return Status::IoError;
                        }
                        if (parent.serialize(parent_guard.pageMut().body()) != Status::Ok) return Status::IoError;
                        node_guard.markDirty();
                        sibling_guard.markDirty();
                        parent_guard.markDirty();
                        return Status::Ok;
                    }
                }
            }
        }

        // Merge right node into left node
        bool merged_with_left = false;
        int right_node_idx = -1;

        if (left_sibling_id != kInvalidPageId)
        {
            merged_with_left = true;
            right_node_idx = child_idx;
        }
        else
        {
            merged_with_left = false;
            right_node_idx = child_idx + 1;
        }


        WritePageGuard sibling_guard;
        PageId merge_sibling_id = merged_with_left ? left_sibling_id : right_sibling_id;
        if (bpm_.fetchPageWrite(merge_sibling_id, sibling_guard) != Status::Ok)
        {
            return Status::IoError;
        }

        std::span<std::byte> left_body = merged_with_left ? sibling_guard.pageMut().body() : node_guard.pageMut().body();
        std::span<std::byte> right_body = merged_with_left ? node_guard.pageMut().body() : sibling_guard.pageMut().body();

        if (node_header.pageType() == PageType::BTreeLeaf)
        {
            BTreeLeafPage left, right;
            if (left.deserialize(left_body) != Status::Ok) return Status::IoError;
            if (right.deserialize(right_body) != Status::Ok) return Status::IoError;

            right.moveAllTo(left);
            left.setNextPageId(right.nextPageId());

            if (left.nextPageId() != kInvalidPageId)
            {
                WritePageGuard next_guard;
                if (bpm_.fetchPageWrite(left.nextPageId(), next_guard) == Status::Ok)
                {
                    BTreeLeafPage next_leaf;
                    if (next_leaf.deserialize(next_guard.page().body()) == Status::Ok)
                    {
                        next_leaf.setPrevPageId(left.pageId());
                        if (next_leaf.serialize(next_guard.pageMut().body()) != Status::Ok) return Status::IoError;
                        next_guard.markDirty();
                    }
                }
            }

            if (left.serialize(left_body) != Status::Ok) return Status::IoError;
            if (right.serialize(right_body) != Status::Ok) return Status::IoError;
        }
        else
        {
            BTreeInternalPage left, right;
            if (left.deserialize(left_body) != Status::Ok) return Status::IoError;
            if (right.deserialize(right_body) != Status::Ok) return Status::IoError;

            uint16_t right_orig_size = right.size();
            right.moveAllTo(left, parent.keyAt(right_node_idx));

            for (uint16_t i = left.size() - right_orig_size; i < left.size(); ++i)
            {
                PageId moved_child = left.childAt(i);
                WritePageGuard moved_guard;
                if (bpm_.fetchPageWrite(moved_child, moved_guard) == Status::Ok)
                {
                    BTreePage moved_header;
                    if (moved_header.deserialize(moved_guard.page().body()) == Status::Ok)
                    {
                        moved_header.setParentPageId(left.pageId());
                        if (moved_header.serialize(moved_guard.pageMut().body()) != Status::Ok) return Status::IoError;
                        moved_guard.markDirty();
                    }
                }
            }

            if (left.serialize(left_body) != Status::Ok) return Status::IoError;
            if (right.serialize(right_body) != Status::Ok) return Status::IoError;
        }

        if (parent.remove(parent.keyAt(right_node_idx)) != Status::Ok) return Status::IoError;
        if (parent.serialize(parent_guard.pageMut().body()) != Status::Ok) return Status::IoError;

        node_guard.markDirty();
        sibling_guard.markDirty();
        parent_guard.markDirty();

        node_guard.drop();
        sibling_guard.drop();
        
        bool parent_underflow = parent.size() < parent.minSize();
        bool parent_is_empty_root = parent_id == root_page_id_ && parent.size() == 1;

        parent_guard.drop();

        if (parent_is_empty_root)
        {
            return handleUnderflow(parent_id);
        }
        else if (parent_underflow && parent_id != root_page_id_)
        {
            return handleUnderflow(parent_id);
        }

        return Status::Ok;
    }


    BPlusTreeIterator BPlusTree::begin() noexcept
    {
        if (isEmpty())
        {
            return end();
        }

        PageId curr_page_id = root_page_id_;
        while (curr_page_id != kInvalidPageId)
        {
            ReadPageGuard guard;
            if (bpm_.fetchPageRead(curr_page_id, guard) != Status::Ok)
            {
                return end();
            }

            BTreePage header;
            if (header.deserialize(guard.page().body()) != Status::Ok)
            {
                return end();
            }

            if (header.pageType() == PageType::BTreeLeaf)
            {
                BTreeLeafPage leaf;
                if (leaf.deserialize(guard.page().body()) != Status::Ok || leaf.isEmpty())
                {
                    return end();
                }
                guard.drop(); // Destroy before iterator creates its own
                return BPlusTreeIterator(bpm_, curr_page_id, 0);
            }
            else if (header.pageType() == PageType::BTreeInternal)
            {
                BTreeInternalPage internal;
                if (internal.deserialize(guard.page().body()) != Status::Ok)
                {
                    return end();
                }
                curr_page_id = internal.childAt(0); // Leftmost child
            }
            else
            {
                return end();
            }
        }

        return end();
    }

    BPlusTreeIterator BPlusTree::begin(int64_t key) noexcept
    {
        if (isEmpty())
        {
            return end();
        }

        PageId curr_page_id = root_page_id_;
        while (curr_page_id != kInvalidPageId)
        {
            ReadPageGuard guard;
            if (bpm_.fetchPageRead(curr_page_id, guard) != Status::Ok)
            {
                return end();
            }

            BTreePage header;
            if (header.deserialize(guard.page().body()) != Status::Ok)
            {
                return end();
            }

            if (header.pageType() == PageType::BTreeLeaf)
            {
                BTreeLeafPage leaf;
                if (leaf.deserialize(guard.page().body()) != Status::Ok || leaf.isEmpty())
                {
                    return end();
                }

                uint16_t index = 0;
                while (index < leaf.size() && leaf.keyAt(index) < key)
                {
                    index++;
                }

                if (index < leaf.size())
                {
                    guard.drop(); // Destroy before iterator creates its own
                    return BPlusTreeIterator(bpm_, curr_page_id, index);
                }
                else
                {
                    PageId next_page_id = leaf.nextPageId();
                    guard.drop(); // Destroy before iterator creates its own
                    if (next_page_id != kInvalidPageId)
                    {
                        return BPlusTreeIterator(bpm_, next_page_id, 0);
                    }
                    return end();
                }
            }
            else if (header.pageType() == PageType::BTreeInternal)
            {
                BTreeInternalPage internal;
                if (internal.deserialize(guard.page().body()) != Status::Ok)
                {
                    return end();
                }
                curr_page_id = internal.lookup(key);
            }
            else
            {
                return end();
            }
        }

        return end();
    }

    BPlusTreeIterator BPlusTree::end() noexcept
    {
        return BPlusTreeIterator();
    }

} // namespace hamdb
