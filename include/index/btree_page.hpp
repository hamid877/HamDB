#pragma once

/// @file btree_page.hpp
/// @brief Metadata header shared by all B+ Tree node pages.
///
/// @c BTreePage models the common 16-byte header that is written at the start
/// of every B+ Tree page body (i.e. after the generic @c PageHeader).
/// Both internal and leaf node implementations embed this struct and extend it
/// with their own slot arrays.
///
/// On-disk layout (all fields little-endian):
/// | Offset | Size | Field          |
/// |--------|------|----------------|
/// |      0 |    1 | page_type      |
/// |      1 |    2 | current_size   |
/// |      3 |    2 | max_size       |
/// |      5 |    4 | parent_page_id |
/// |      9 |    4 | page_id        |
/// |     13 |    3 | (reserved)     |
///
/// Total serialised size: 16 bytes (== @c BTreePage::kHeaderSize).

#include "common/constants.hpp"
#include "common/enums.hpp"
#include "utils/deserializer.hpp"
#include "utils/serializer.hpp"
#include <cstdint>
#include <span>

namespace hamdb
{

    /**
     * @brief Metadata header shared by every B+ Tree node page.
     *
     * @c BTreePage stores the bookkeeping fields common to both internal and
     * leaf nodes: page type, occupancy counters, parent linkage, and page
     * identity.  It does not hold any key/value data.
     *
     * The serialised form is exactly @c kHeaderSize bytes long, enabling both
     * internal and leaf pages to locate their slot arrays at a fixed, compile-
     * time-known offset.
     *
     * @note Serialisation uses the project @c Serializer / @c Deserializer
     *       utilities (field-by-field, little-endian).  Never cast this struct
     *       to raw bytes.
     */
    class BTreePage
    {
    public:
        /// Serialised size of the B+ Tree page header in bytes.
        static constexpr std::size_t kHeaderSize = 16;

        // ── Construction ──────────────────────────────────────────────────────────

        /**
         * @brief Default-construct an uninitialised B+ Tree page header.
         *
         * All fields are set to their zero / sentinel values.  Callers must
         * populate every field before serialising.
         */
        BTreePage() noexcept = default;

        /**
         * @brief Construct a fully initialised B+ Tree page header.
         *
         * @param type           Page type — must be @c PageType::BTreeInternal
         *                       or @c PageType::BTreeLeaf.
         * @param page_id        Logical identifier of this page.
         * @param parent_page_id Logical identifier of the parent node, or
         *                       @c kInvalidPageId for the root.
         * @param max_size       Maximum number of entries this page can hold.
         */
        BTreePage(PageType type,
                  PageId   page_id,
                  PageId   parent_page_id,
                  uint16_t max_size) noexcept;

        // ── Getters ───────────────────────────────────────────────────────────────

        /**
         * @brief Return the type of this B+ Tree page.
         * @return @c PageType::BTreeInternal or @c PageType::BTreeLeaf.
         */
        [[nodiscard]] PageType pageType() const noexcept;

        /**
         * @brief Return the current number of entries stored in this page.
         *
         * For leaf pages this is the number of key/value pairs; for internal
         * pages it is the number of child pointers.
         */
        [[nodiscard]] uint16_t currentSize() const noexcept;

        /**
         * @brief Return the maximum number of entries this page can hold.
         *
         * A page is considered full when @c currentSize() == @c maxSize().
         */
        [[nodiscard]] uint16_t maxSize() const noexcept;

        /**
         * @brief Return the minimum number of entries this page must hold.
         *
         * Follows B+ Tree occupancy rules.
         */
        [[nodiscard]] uint16_t minSize() const noexcept;


        /**
         * @brief Return the logical page ID of the parent node.
         * @return @c kInvalidPageId if this page is the B+ Tree root.
         */
        [[nodiscard]] PageId parentPageId() const noexcept;

        /**
         * @brief Return the logical page ID of this node.
         */
        [[nodiscard]] PageId pageId() const noexcept;

        /**
         * @brief Return @c true if the page has no free entry slots.
         */
        [[nodiscard]] bool isFull() const noexcept;

        /**
         * @brief Return @c true if this page is the root of the B+ Tree.
         *
         * A page is a root when its parent ID equals @c kInvalidPageId.
         */
        [[nodiscard]] bool isRoot() const noexcept;

        // ── Setters ───────────────────────────────────────────────────────────────

        /**
         * @brief Set the page type.
         * @param type Must be @c PageType::BTreeInternal or @c PageType::BTreeLeaf.
         */
        void setPageType(PageType type) noexcept;

        /**
         * @brief Set the current occupancy count.
         * @param size New entry count.  Must not exceed @c maxSize().
         */
        void setCurrentSize(uint16_t size) noexcept;

        /**
         * @brief Set the maximum capacity of this page.
         * @param size Maximum entry count.
         */
        void setMaxSize(uint16_t size) noexcept;

        /**
         * @brief Set the parent page identifier.
         * @param id Logical page ID of the parent, or @c kInvalidPageId for root.
         */
        void setParentPageId(PageId id) noexcept;

        /**
         * @brief Set the page identifier.
         * @param id Logical page ID assigned by the buffer pool.
         */
        void setPageId(PageId id) noexcept;

        // ── Serialisation ─────────────────────────────────────────────────────────

        /**
         * @brief Serialise the header into @p dest.
         *
         * Writes exactly @c kHeaderSize bytes in little-endian order.  The three
         * reserved bytes at the end are zero-filled.
         *
         * @param dest Writable span of at least @c kHeaderSize bytes.
         * @return @c Status::Ok on success, @c Status::IoError if @p dest is too
         *         small.
         */
        [[nodiscard]] Status serialize(std::span<std::byte> dest) const noexcept;

        /**
         * @brief Deserialise the header from @p src.
         *
         * Reads exactly @c kHeaderSize bytes.  All fields are overwritten on
         * success; on failure no field is modified.
         *
         * @param src Read-only span of at least @c kHeaderSize bytes.
         * @return @c Status::Ok on success, @c Status::IoError if @p src is too
         *         short.
         */
        [[nodiscard]] Status deserialize(std::span<const std::byte> src) noexcept;

        // ── Comparison ────────────────────────────────────────────────────────────

        /// Equality — compares all meaningful fields (reserved bytes ignored).
        [[nodiscard]] bool operator==(const BTreePage& other) const noexcept;

        /// Inequality.
        [[nodiscard]] bool operator!=(const BTreePage& other) const noexcept;

    private:
        PageType page_type_      = PageType::BTreeLeaf; ///< Internal or leaf node.
        uint16_t current_size_   = 0;                   ///< Current entry count.
        uint16_t max_size_       = 0;                   ///< Maximum entry capacity.
        PageId   parent_page_id_ = kInvalidPageId;      ///< Parent node ID.
        PageId   page_id_        = kInvalidPageId;      ///< This node's page ID.
    };

} // namespace hamdb
