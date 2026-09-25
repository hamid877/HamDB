#pragma once

#include "buffer/buffer_pool_manager.hpp"
#include "index/btree_leaf_page.hpp"
#include "storage/rid.hpp"

#include <optional>
#include <utility>

namespace hamdb
{

    /**
     * @brief Iterator for sequential traversal of B+ Tree leaf pages.
     */
    class BPlusTreeIterator
    {
    public:
        /**
         * @brief Default constructor creating an end iterator.
         */
        BPlusTreeIterator() = default;

        /**
         * @brief Construct an iterator starting at the given page and slot index.
         *
         * @param bpm The buffer pool manager.
         * @param page_id The starting leaf page ID.
         * @param index The starting slot index within the leaf page.
         */
        BPlusTreeIterator(BufferPoolManager& bpm, PageId page_id, uint16_t index) noexcept;

        /**
         * @brief Move constructor.
         */
        BPlusTreeIterator(BPlusTreeIterator&& other) noexcept;

        /**
         * @brief Move assignment operator.
         */
        BPlusTreeIterator& operator=(BPlusTreeIterator&& other) noexcept;

        /**
         * @brief Check if this iterator represents the end of the tree.
         *
         * @return true if at the end, false otherwise.
         */
        [[nodiscard]] bool isEnd() const noexcept;

        /**
         * @brief Dereference the iterator to get the current key/value pair.
         *
         * @return A pair containing the current key and RID.
         */
        [[nodiscard]] std::pair<int64_t, RID> operator*() const noexcept;

        /**
         * @brief Prefix increment operator to advance the iterator.
         *
         * @return A reference to the advanced iterator.
         */
        BPlusTreeIterator& operator++() noexcept;

        /**
         * @brief Equality comparison operator.
         *
         * @param other The iterator to compare with.
         * @return true if both iterators point to the same element, false otherwise.
         */
        [[nodiscard]] bool operator==(const BPlusTreeIterator& other) const noexcept;

        /**
         * @brief Inequality comparison operator.
         *
         * @param other The iterator to compare with.
         * @return true if the iterators point to different elements, false otherwise.
         */
        [[nodiscard]] bool operator!=(const BPlusTreeIterator& other) const noexcept;

    private:
        /**
         * @brief Load the leaf page at page_id_ into memory.
         */
        void loadPage() noexcept;

        BufferPoolManager* bpm_{nullptr};
        PageId page_id_{kInvalidPageId};
        uint16_t index_{0};
        std::optional<ReadPageGuard> guard_{std::nullopt};
        BTreeLeafPage leaf_{};
    };

} // namespace hamdb
