#pragma once

#include "buffer/buffer_pool_manager.hpp"
#include "common/constants.hpp"
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
         * @return Status::Ok on success, Status::AlreadyExists if duplicate, Status::PageFull if full.
         */
        [[nodiscard]] Status insert(int64_t key, RID rid) noexcept;

    private:
        BufferPoolManager& bpm_;
        PageId             root_page_id_{kInvalidPageId};
    };

} // namespace hamdb
