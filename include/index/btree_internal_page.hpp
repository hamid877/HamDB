#pragma once

/// @file btree_internal_page.hpp
/// @brief B+ Tree internal page for routing searches.
///
/// @c BTreeInternalPage extends the shared @c BTreePage header with a fixed-size
/// array of @c <int64_t, PageId> entries (InternalEntry) to act as search routers.
///
/// In an internal node with N keys, there are N + 1 child pointers.
/// We store this as an array of entries where `entries[0].key` is unused,
/// and `entries[0].page_id` is the leftmost child. For `i >= 1`, `entries[i].key`
/// is the separator such that all keys in `entries[i-1].page_id` are strictly
/// less than `entries[i].key`, and keys in `entries[i].page_id` are greater
/// than or equal to `entries[i].key`.
///
/// On-disk layout (offsets relative to start of the page body):
/// | Offset | Size | Field                          |
/// |--------|------|--------------------------------|
/// |      0 |   16 | BTreePage header (base class)  |
/// |     16 |    8 | entries[0].key (unused)        |
/// |     24 |    4 | entries[0].page_id             |
/// |     28 |   12 | entries[1] …                   |
/// |    …   |    … | …                              |

#include "common/constants.hpp"
#include "common/enums.hpp"
#include "index/btree_page.hpp"
#include "utils/deserializer.hpp"
#include "utils/serializer.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace hamdb
{

    /**
     * @brief A single separator key and its right-child pointer in an internal page.
     */
    struct InternalEntry
    {
        int64_t key{0};
        PageId  page_id{kInvalidPageId};
    };

    /**
     * @brief B+ Tree internal page storing separator keys and child pointers.
     */
    class BTreeInternalPage
    {
    public:
        // ── Layout constants ───────────────────────────────────────────────────────

        static constexpr std::size_t kHeaderSize = BTreePage::kHeaderSize; // 16
        static constexpr std::size_t kEntrySize = 8 + 4;                   // 12

        static constexpr std::size_t kMaxEntries =
            (kPageBodySize - kHeaderSize) / kEntrySize;

        // ── Construction ───────────────────────────────────────────────────────────

        BTreeInternalPage() noexcept = default;

        /**
         * @brief Initialise an empty internal page.
         *
         * Sets the base @c BTreePage header fields. The page will have 0 children.
         */
        void init(PageId page_id, PageId parent_page_id) noexcept;

        // ── Delegated BTreePage accessors ──────────────────────────────────────────

        /**
         * @brief Return the current number of child pointers in this page.
         *
         * The number of keys is `size() - 1` (if size > 0).
         */
        [[nodiscard]] uint16_t size() const noexcept;

        /// Return the maximum number of child pointers this page can hold.
        [[nodiscard]] uint16_t maxSize() const noexcept;

        /// Return @c true when no children are stored.
        [[nodiscard]] bool isEmpty() const noexcept;

        /// Return @c true when @c size() == @c maxSize() and no more entries fit.
        [[nodiscard]] bool isFull() const noexcept;

        /// Return the logical page ID of this node.
        [[nodiscard]] PageId pageId() const noexcept;

        /// Return the logical page ID of the parent node.
        [[nodiscard]] PageId parentPageId() const noexcept;

        // ── Slot accessors ─────────────────────────────────────────────────────────

        /**
         * @brief Return the key stored at slot @p index.
         *
         * @param index Zero-based slot index. Must be >= 1 and < @c size().
         */
        [[nodiscard]] int64_t keyAt(uint16_t index) const noexcept;

        /**
         * @brief Return the child PageId stored at slot @p index.
         *
         * @param index Zero-based slot index. Must be < @c size().
         */
        [[nodiscard]] PageId childAt(uint16_t index) const noexcept;

        // ── Lookup ─────────────────────────────────────────────────────────────────

        /**
         * @brief Binary search for the child that should contain @p key.
         *
         * @param key Search key.
         * @return The @c PageId of the child router. Returns @c kInvalidPageId if empty.
         */
        [[nodiscard]] PageId lookup(int64_t key) const noexcept;

        // ── Mutation ───────────────────────────────────────────────────────────────

        /**
         * @brief Populate an empty internal page as a new root.
         *
         * Sets up the page to have exactly 2 children and 1 separator key.
         */
        void populateNewRoot(PageId left_child, int64_t key, PageId right_child) noexcept;

        /**
         * @brief Insert a new separator key and its right-child pointer.
         *
         * Maintains sorted order of keys. Duplicates are rejected.
         *
         * @param key Separator key to insert.
         * @param child_page_id The child page ID corresponding to this key.
         * @return @c Status::Ok on success.
         * @return @c Status::AlreadyExists if the key is already present.
         * @return @c Status::InvalidArg if the page is full.
         */
        [[nodiscard]] Status insert(int64_t key, PageId child_page_id) noexcept;

        /**
         * @brief Remove the separator key and its associated right-child pointer.
         *
         * Shifts remaining entries left to fill the gap.
         *
         * @param key Key to remove.
         * @return @c Status::Ok on success.
         * @return @c Status::NotFound if the key is not in this internal page.
         */
        [[nodiscard]] Status remove(int64_t key) noexcept;

        /**
         * @brief Move the upper half of the entries to a recipient page.
         * 
         * The median key is returned and removed from both pages.
         * This function also updates the parent_page_id of the moved children.
         * 
         * @param recipient The sibling page that will receive the upper half.
         * @param bpm The buffer pool manager used to fetch children and update their parent pointer.
         * @return The median key that should be promoted to the parent.
         */
        int64_t moveHalfTo(BTreeInternalPage& recipient, class BufferPoolManager& bpm) noexcept;

        // ── Serialisation ──────────────────────────────────────────────────────────

        /**
         * @brief Serialise the complete internal page to @p dest.
         */
        [[nodiscard]] Status serialize(std::span<std::byte> dest) const noexcept;

        /**
         * @brief Deserialise a previously serialised internal page from @p src.
         */
        [[nodiscard]] Status deserialize(std::span<const std::byte> src) noexcept;

    private:
        static Status serializeEntry(Serializer& ser, const InternalEntry& entry) noexcept;
        static Status deserializeEntry(Deserializer& de, InternalEntry& entry) noexcept;

        BTreePage  header_{};
        std::array<InternalEntry, kMaxEntries> entries_{};
    };

} // namespace hamdb
