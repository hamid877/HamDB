#pragma once

#include "buffer/buffer_pool_manager.hpp"
#include "common/constants.hpp"
#include "index/bplus_tree_iterator.hpp"
#include "storage/rid.hpp"

#include <optional>

namespace hamdb
{

    /**
     * @brief Read-only B+ Tree implementation for searching.
     */
    class BPlusTree
    {
    public:
        /**
         * @brief Construct a new BPlusTree.
         *
         * @param bpm The buffer pool manager used to fetch pages.
         */
        explicit BPlusTree(BufferPoolManager& bpm) noexcept;

        /**
         * @brief Initialize an empty tree.
         */
        void create() noexcept;

        /**
         * @brief Open an existing tree.
         *
         * @param root_page_id The root page ID of the existing tree.
         */
        void open(PageId root_page_id) noexcept;

        /**
         * @brief Check if the tree is empty.
         *
         * @return true if the root page ID is invalid, false otherwise.
         */
        [[nodiscard]] bool isEmpty() const noexcept;

        /**
         * @brief Search for a key in the B+ Tree.
         *
         * Traverses from the root to a leaf page to find the value.
         *
         * @param key The key to search for.
         * @return The RID associated with the key, or std::nullopt if not found.
         */
        [[nodiscard]] std::optional<RID> getValue(int64_t key) noexcept;

        /**
         * @brief Insert a key/value pair into the B+ Tree.
         *
         * @param key The key to insert.
         * @param rid The RID to insert.
         * @return Status::Ok on success, Status::AlreadyExists if duplicate, Status::PageFull if
         * full.
         */
        [[nodiscard]] Status insert(int64_t key, RID rid) noexcept;

        /**
         * @brief Remove a key/value pair from the B+ Tree.
         *
         * @param key The key to remove.
         * @return Status::Ok on success, Status::NotFound if not found.
         */
        [[nodiscard]] Status remove(int64_t key) noexcept;

        /**
         * @brief Get an iterator pointing to the first key in the B+ tree.
         *
         * @return BPlusTreeIterator pointing to the first key/value pair.
         */
        [[nodiscard]] BPlusTreeIterator begin() noexcept;

        /**
         * @brief Get an iterator pointing to the first key >= the provided key.
         *
         * @param key The lower bound key.
         * @return BPlusTreeIterator pointing to the key/value pair.
         */
        [[nodiscard]] BPlusTreeIterator begin(int64_t key) noexcept;

        /**
         * @brief Get an iterator representing the end of the tree.
         *
         * @return BPlusTreeIterator pointing past the last key.
         */
        [[nodiscard]] BPlusTreeIterator end() noexcept;

    private:
        /**
         * @brief Recursively insert a new child into the parent page.
         *
         * @param old_node_id The child page that was split.
         * @param key The separator key.
         * @param new_node_id The new child page created by the split.
         * @return Status::Ok on success.
         */
        [[nodiscard]] Status insertIntoParent(PageId old_node_id, int64_t key,
                                              PageId new_node_id) noexcept;

        /**
         * @brief Recursively handle underflow for a given page.
         *
         * @param page_id The ID of the page that has underflowed.
         * @return Status::Ok on success.
         */
        [[nodiscard]] Status handleUnderflow(PageId page_id) noexcept;

        BufferPoolManager& bpm_;
        PageId root_page_id_{kInvalidPageId};
    };

} // namespace hamdb
