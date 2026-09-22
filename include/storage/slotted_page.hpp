#pragma once

/// @file slotted_page.hpp
/// @brief Slotted-page storage format for variable-length tuples.
///
/// A @c SlottedPage wraps an existing @c Page and organises its body as:
///
/// @code
/// Body (4080 bytes)
/// ┌────────────────────────────────────────────────────────────────┐
/// │ SlottedPageHeader  [0..7]     8 bytes                         │
/// │ Slot[0]            [8..15]    8 bytes each                    │
/// │ Slot[1]            [16..23]                                   │
/// │ ...                                                           │
/// │ ← freeSpaceStart grows down                                   │
/// │                   [FREE ZONE]                                 │
/// │                              freeSpaceEnd shrinks up ←       │
/// │ ...                                                           │
/// │ Tuple data        [grows toward lower offsets from body end]  │
/// └────────────────────────────────────────────────────────────────┘
/// @endcode
///
/// All byte addresses are relative to the **start of the body** (i.e. offset
/// 0 = first byte after the 16-byte @c PageHeader).

#include "common/enums.hpp"
#include "storage/page.hpp"
#include "storage/tuple_slot.hpp"
#include <cstdint>
#include <optional>
#include <span>

namespace hamdb
{

    /// Logical identifier for a slot within a page (0-indexed).
    using SlotId = std::uint16_t;

    /// Sentinel value for an invalid slot ID.
    inline constexpr SlotId kInvalidSlotId = UINT16_MAX;

    // ── SlottedPageHeader ─────────────────────────────────────────────────────

    /**
     * @brief 8-byte bookkeeping record stored at the very start of the page body.
     *
     * This header is written and read via @c Serializer / @c Deserializer so
     * there is no unsafe pointer aliasing.
     *
     * Layout (all little-endian):
     * | Offset | Size | Field           |
     * |--------|------|-----------------|
     * |      0 |    2 | tuple_count     |
     * |      2 |    2 | freeSpaceStart  |
     * |      4 |    2 | freeSpaceEnd    |
     * |      6 |    2 | reserved        |
     */
    struct SlottedPageHeader
    {
        /// Serialised size in bytes.
        static constexpr std::size_t kSize = 8;

        /// Number of live (non-deleted) tuples.
        std::uint16_t tuple_count = 0;

        /// Byte offset (from body start) of the first free byte in the slot dir.
        std::uint16_t free_space_start = 0;

        /// Byte offset (from body start) of the last free byte + 1 in the tuple
        /// area (i.e. tuples occupy [freeSpaceEnd .. bodySize)).
        std::uint16_t free_space_end = 0;

        /// Reserved; must be zero.
        std::uint16_t reserved = 0;
    };

    // ── SlottedPage ───────────────────────────────────────────────────────────

    /**
     * @brief Organises a @c Page body as a slotted-page record store.
     *
     * @c SlottedPage does **not** own the backing @c Page; the caller is
     * responsible for keeping the @c Page alive.  No heap allocation is
     * performed inside this class.
     *
     * Slot indices (slot IDs) are stable: a deleted slot is never reclaimed
     * until @c compact() is called.  Re-insertion may reuse a deleted slot
     * before creating a new one.
     *
     * @note Not thread-safe.
     */
    class SlottedPage
    {
    public:
        // ── Constants ─────────────────────────────────────────────────────────

        /// Size of the body area available to @c SlottedPage.
        static constexpr std::size_t kBodySize = Page::kBodySize;

        /// Minimum free space required to insert any tuple.
        static constexpr std::size_t kMinInsertBytes =
            SlottedPageHeader::kSize + TupleSlot::kSize + 1;

        // ── Construction ──────────────────────────────────────────────────────

        /**
         * @brief Attach a @c SlottedPage view to an existing @c Page.
         *
         * Does **not** initialise the page.  Call @c initialize() for a fresh
         * page, or simply call read/write methods on an already-initialised one.
         *
         * @param page Reference to the backing @c Page (must outlive this object).
         */
        explicit SlottedPage(Page& page) noexcept;

        // ── Lifecycle ─────────────────────────────────────────────────────────

        /**
         * @brief Initialise the body as an empty slotted page.
         *
         * Zeros the body, writes a fresh @c SlottedPageHeader, and updates
         * the @c PageHeader.  Safe to call on a freshly constructed @c Page.
         *
         * @return @c Status::Ok always.
         */
        [[nodiscard]] Status initialize() noexcept;

        // ── Mutation ──────────────────────────────────────────────────────────

        /**
         * @brief Insert a tuple into the page and return its slot ID.
         *
         * The tuple bytes are copied into the tuple area at the high end of the
         * body.  A slot entry is created (or a deleted slot is reused) in the
         * slot directory at the low end.
         *
         * @param tuple   Raw tuple bytes to store.
         * @param[out] slot_id  Receives the assigned slot ID on success.
         * @return @c Status::Ok, or @c Status::IoError if the page is full.
         */
        [[nodiscard]] Status insertTuple(std::span<const std::byte> tuple,
                                         SlotId& slot_id) noexcept;

        /**
         * @brief Mark the tuple in slot @p slot_id as deleted.
         *
         * The tuple bytes are **not** erased; call @c compact() to reclaim space.
         * The live tuple count is decremented.
         *
         * @return @c Status::Ok, @c Status::NotFound if the slot does not exist,
         *         @c Status::InvalidArg if the slot is already deleted.
         */
        [[nodiscard]] Status deleteTuple(SlotId slot_id) noexcept;

        /**
         * @brief Compact the page, reclaiming space from deleted tuples.
         *
         * Active tuples are moved toward the end of the body; slot offsets are
         * updated accordingly.  Slot IDs are preserved.
         *
         * @param[out] reclaimed_bytes Bytes recovered (may be zero).
         * @return @c Status::Ok always.
         */
        [[nodiscard]] Status compact(std::size_t& reclaimed_bytes) noexcept;

        // ── Queries ───────────────────────────────────────────────────────────

        /**
         * @brief Read the raw bytes of the tuple in slot @p slot_id.
         *
         * Returns a non-owning view into the backing page body.  Valid only
         * while the @c Page lives and is not compacted.
         *
         * @param slot_id     Slot to read.
         * @param[out] tuple  Set to a span over the tuple bytes on success.
         * @return @c Status::Ok, @c Status::NotFound if the slot is out of range,
         *         @c Status::InvalidArg if the slot is deleted.
         */
        [[nodiscard]] Status readTuple(SlotId slot_id,
                                       std::span<const std::byte>& tuple) const noexcept;

        /// Return the number of free bytes currently available for new data.
        [[nodiscard]] std::size_t freeSpace() const noexcept;

        /// Return the total number of slot entries (including deleted ones).
        [[nodiscard]] std::uint16_t slotCount() const noexcept;

        /// Return the number of live (non-deleted) tuples.
        [[nodiscard]] std::uint16_t tupleCount() const noexcept;

        /**
         * @brief Return true if a tuple of @p tuple_size bytes cannot be inserted.
         *
         * Accounts for the slot directory entry as well as the tuple payload.
         */
        [[nodiscard]] bool isFull(std::size_t tuple_size) const noexcept;

    private:
        Page& page_; ///< Reference to the backing page (non-owning).

        // ── Internal helpers ──────────────────────────────────────────────────

        /// Read the SlottedPageHeader from the body.
        [[nodiscard]] SlottedPageHeader readSpHeader() const noexcept;

        /// Write the SlottedPageHeader back to the body.
        void writeSpHeader(const SlottedPageHeader& h) noexcept;

        /// Read TupleSlot @p idx from the slot directory.
        [[nodiscard]] TupleSlot readSlot(std::uint16_t idx) const noexcept;

        /// Write TupleSlot @p idx into the slot directory.
        void writeSlot(std::uint16_t idx, const TupleSlot& slot) noexcept;

        /// Byte offset in the body where slot @p idx begins.
        static constexpr std::size_t slotOffset(std::uint16_t idx) noexcept
        {
            return SlottedPageHeader::kSize + static_cast<std::size_t>(idx) * TupleSlot::kSize;
        }
    };

} // namespace hamdb
