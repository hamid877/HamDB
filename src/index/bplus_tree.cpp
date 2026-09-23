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

} // namespace hamdb
