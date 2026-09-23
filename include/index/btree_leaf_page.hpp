#pragma once

/// @file btree_leaf_page.hpp
/// @brief B+ Tree leaf page storing sorted key/RID pairs.
///
/// @c BTreeLeafPage extends the shared @c BTreePage header with two sibling
/// page pointers (@c prev_page_id_ and @c next_page_id_) and a fixed-size
/// sorted array of @c <int64_t, RID> entries packed directly into the 4 KiB
/// page body.
///
/// On-disk layout (offsets relative to start of the page body):
/// | Offset | Size | Field                          |
/// |--------|------|--------------------------------|
/// |      0 |   16 | BTreePage header (base class)  |
/// |     16 |    4 | prev_page_id                   |
/// |     20 |    4 | next_page_id                   |
/// |     24 |    8 | entries[0].key   (int64, LE)   |
/// |     32 |    4 | entries[0].rid.page_id (uint32)|
/// |     36 |    2 | entries[0].rid.slot_id (uint16)|
/// |     38 |   14 | entries[1] …                   |
/// |    …   |    … | …                              |
///
/// Entry size: 14 bytes (8 key + 4 page_id + 2 slot_id).
/// Leaf-header size: 24 bytes (16 BTreePage + 4 prev + 4 next).
/// Maximum entries: (kPageBodySize - kLeafHeaderSize) / kEntrySize.

#include "common/constants.hpp"
#include "common/enums.hpp"
#include "index/btree_page.hpp"
#include "storage/rid.hpp"
#include "utils/deserializer.hpp"
#include "utils/serializer.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace hamdb
{

    /**
     * @brief A single sorted key/value entry stored in a B+ Tree leaf page.
     *
     * The key is a 64-bit signed integer; the value is a @c RID pointing
     * to the heap tuple associated with that key.
     */
    struct LeafEntry
    {
        int64_t key{0};  ///< Sorted index key.
        RID     rid{};   ///< Heap location of the corresponding tuple.
    };

    /**
     * @brief B+ Tree leaf page: sorted array of @c <int64_t, RID> entries.
     *
     * @c BTreeLeafPage operates over a caller-owned 4 KiB byte buffer (the
     * page *body* returned by @c Page::body()).  It does not allocate memory,
     * store a separate key array, or depend on the @c Page class directly —
     * instead callers serialise/deserialise to and from the buffer.
     *
     * The in-memory representation keeps entries in a @c std::array sorted by
     * key.  @c insert() maintains sorted order by shifting entries right;
     * @c remove() shifts entries left after deletion.
     *
     * Sibling pointers (@c prevPageId / @c nextPageId) enable forward and
     * backward iteration across leaf pages without traversing the tree.
     *
     * @note No page splitting is performed here.  When @c isFull() returns
     *       @c true the caller is responsible for requesting a split.
     *
     * @note Not thread-safe.
     */
    class BTreeLeafPage
    {
    public:
        // ── Layout constants ───────────────────────────────────────────────────────

        /// Serialised size of the shared B+ Tree header portion (bytes).
        static constexpr std::size_t kBTreeHeaderSize = BTreePage::kHeaderSize; // 16

        /// Extra bytes for prev/next sibling pointers (2 × uint32).
        static constexpr std::size_t kSiblingPtrSize = 8;

        /// Total serialised leaf header size in bytes.
        static constexpr std::size_t kLeafHeaderSize = kBTreeHeaderSize + kSiblingPtrSize; // 24

        /// Serialised size of one entry: int64 key + uint32 page_id + uint16 slot_id.
        static constexpr std::size_t kEntrySize = 8 + 4 + 2; // 14

        /// Maximum number of entries that fit into a single leaf page.
        static constexpr std::size_t kMaxEntries =
            (kPageBodySize - kLeafHeaderSize) / kEntrySize;

        // ── Construction ───────────────────────────────────────────────────────────

        /**
         * @brief Default-construct an uninitialised leaf page.
         *
         * All fields take sentinel / zero values.  Call @c init() or
         * @c deserialize() before using the page.
         */
        BTreeLeafPage() noexcept = default;

        /**
         * @brief Initialise a fresh, empty leaf page.
         *
         * Sets the base @c BTreePage header fields and clears sibling pointers.
         * After this call the page contains zero entries and @c isEmpty() is
         * @c true.
         *
         * @param page_id        Logical ID of this page.
         * @param parent_page_id Logical ID of the parent node, or
         *                       @c kInvalidPageId for a root leaf.
         */
        void init(PageId page_id, PageId parent_page_id) noexcept;

        // ── Header accessors ───────────────────────────────────────────────────────

        /**
         * @brief Return the logical page ID of the previous sibling leaf.
         * @return @c kInvalidPageId if this is the first leaf.
         */
        [[nodiscard]] PageId prevPageId() const noexcept;

        /**
         * @brief Return the logical page ID of the next sibling leaf.
         * @return @c kInvalidPageId if this is the last leaf.
         */
        [[nodiscard]] PageId nextPageId() const noexcept;

        /**
         * @brief Set the previous sibling page ID.
         * @param id Logical page ID of the predecessor leaf, or
         *           @c kInvalidPageId if none.
         */
        void setPrevPageId(PageId id) noexcept;

        /**
         * @brief Set the next sibling page ID.
         * @param id Logical page ID of the successor leaf, or
         *           @c kInvalidPageId if none.
         */
        void setNextPageId(PageId id) noexcept;

        // ── Delegated BTreePage accessors ──────────────────────────────────────────

        /// Return the current number of key/RID pairs stored in this leaf.
        [[nodiscard]] uint16_t size() const noexcept;

        /// Return the maximum number of key/RID pairs this leaf can hold.
        [[nodiscard]] uint16_t maxSize() const noexcept;

        /// Return the minimum number of key/RID pairs this leaf must hold.
        [[nodiscard]] uint16_t minSize() const noexcept;


        /// Return @c true when no entries are stored.
        [[nodiscard]] bool isEmpty() const noexcept;

        /// Return @c true when @c size() == @c maxSize() and no more entries fit.
        [[nodiscard]] bool isFull() const noexcept;

        /// Return the logical page ID of this leaf node.
        [[nodiscard]] PageId pageId() const noexcept;

        /// Return the logical page ID of the parent node.
        [[nodiscard]] PageId parentPageId() const noexcept;

        /// Set the logical page ID of the parent node.
        void setParentPageId(PageId id) noexcept;

        // ── Slot accessors ─────────────────────────────────────────────────────────

        /**
         * @brief Return the key stored at slot @p index.
         *
         * @param index Zero-based slot index.  Must be less than @c size().
         * @return The key value at that slot.
         * @pre @p index < size()
         */
        [[nodiscard]] int64_t keyAt(uint16_t index) const noexcept;

        /**
         * @brief Return the RID stored at slot @p index.
         *
         * @param index Zero-based slot index.  Must be less than @c size().
         * @return The @c RID at that slot.
         * @pre @p index < size()
         */
        [[nodiscard]] RID valueAt(uint16_t index) const noexcept;

        // ── Lookup ─────────────────────────────────────────────────────────────────

        /**
         * @brief Binary search for @p key.
         *
         * @param key Key to locate.
         * @return The @c RID associated with @p key, or @c std::nullopt if the
         *         key is not present in this leaf.
         */
        [[nodiscard]] std::optional<RID> lookup(int64_t key) const noexcept;

        // ── Mutation ───────────────────────────────────────────────────────────────

        /**
         * @brief Insert a new @c <key, rid> entry in sorted order.
         *
         * The entry array is kept sorted by key at all times.  Duplicate keys
         * are rejected.
         *
         * @param key Key to insert.
         * @param rid Record identifier of the associated heap tuple.
         * @return @c Status::Ok        — entry inserted successfully.
         * @return @c Status::AlreadyExists — @p key already present in this leaf.
         * @return @c Status::InvalidArg   — page is full (@c isFull() true).
         */
        [[nodiscard]] Status insert(int64_t key, RID rid) noexcept;

        /**
         * @brief Remove the entry with key @p key.
         *
         * Entries to the right of the removed slot are shifted left to close
         * the gap.
         *
         * @param key Key to remove.
         * @return @c Status::Ok       — entry removed.
         * @return @c Status::NotFound — @p key was not present.
         */
        [[nodiscard]] Status remove(int64_t key) noexcept;

        /**
         * @brief Move half of the entries to the recipient page.
         *
         * @param recipient The leaf page to move the upper half of entries to.
         */
        void moveHalfTo(BTreeLeafPage& recipient) noexcept;

        /**
         * @brief Move the first entry of this page to the end of the recipient page.
         */
        void moveFirstToEndOf(BTreeLeafPage& recipient) noexcept;

        /**
         * @brief Move the last entry of this page to the front of the recipient page.
         */
        void moveLastToFrontOf(BTreeLeafPage& recipient) noexcept;

        /**
         * @brief Move all entries of this page to the end of the recipient page.
         */
        void moveAllTo(BTreeLeafPage& recipient) noexcept;


        // ── Serialisation ──────────────────────────────────────────────────────────

        /**
         * @brief Serialise the complete leaf page (header + all entries) into
         *        @p dest.
         *
         * The caller must supply a buffer of at least
         * @c kLeafHeaderSize + @c size() * @c kEntrySize bytes (in practice
         * @c kPageBodySize is always sufficient).
         *
         * @param dest Writable byte span (at least @c kLeafHeaderSize bytes).
         * @return @c Status::Ok or @c Status::IoError if @p dest is too small.
         */
        [[nodiscard]] Status serialize(std::span<std::byte> dest) const noexcept;

        /**
         * @brief Deserialise a previously serialised leaf page from @p src.
         *
         * All fields (header, sibling pointers, and entries) are overwritten on
         * success.  On failure no field is modified.
         *
         * @param src Read-only byte span (at least @c kLeafHeaderSize bytes).
         * @return @c Status::Ok or @c Status::IoError if @p src is too small or
         *         corrupt.
         */
        [[nodiscard]] Status deserialize(std::span<const std::byte> src) noexcept;

    private:
        // ── Private helpers ───────────────────────────────────────────────────────

        /// Perform binary search for @p key; return the slot index or -1.
        [[nodiscard]] int32_t findIndex(int64_t key) const noexcept;

        /// Serialise a single entry at position @p pos in @p ser.
        [[nodiscard]] static Status serializeEntry(Serializer&       ser,
                                                   const LeafEntry&  entry) noexcept;

        /// Deserialise a single entry from @p de into @p entry.
        [[nodiscard]] static Status deserializeEntry(Deserializer& de,
                                                     LeafEntry&    entry) noexcept;

        // ── Data members ──────────────────────────────────────────────────────────

        BTreePage  header_{};                        ///< Shared B+ Tree header.
        PageId     prev_page_id_ = kInvalidPageId;   ///< Predecessor leaf page.
        PageId     next_page_id_ = kInvalidPageId;   ///< Successor leaf page.

        /// In-memory slot array (only the first @c size() entries are valid).
        std::array<LeafEntry, kMaxEntries> entries_{};
    };

} // namespace hamdb
