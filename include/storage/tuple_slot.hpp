#pragma once

/// @file tuple_slot.hpp
/// @brief Slot directory entry for the slotted-page storage format.
///
/// Each @c TupleSlot occupies exactly 8 bytes in the slot directory at the
/// beginning of a page body.  It records the byte offset, length, and status
/// flags of one tuple stored in that page.
///
/// On-disk layout (little-endian):
/// | Offset | Size | Field    | Description                        |
/// |--------|------|----------|------------------------------------|
/// |      0 |    2 | offset   | Byte offset from body start        |
/// |      2 |    2 | length   | Tuple data length in bytes         |
/// |      4 |    2 | flags    | Status flags (see below)           |
/// |      6 |    2 | reserved | Must be zero                       |
///
/// Flag bits:
///   Bit 0 — DELETED: tuple has been logically deleted.

#include <cstdint>

namespace hamdb
{

    /**
     * @brief 8-byte slot directory entry describing one tuple's location.
     *
     * @c TupleSlot is stored directly inside the page body and is never heap-
     * allocated.  All field access is via plain member reads/writes.
     */
    struct TupleSlot
    {
        // ── Constants ─────────────────────────────────────────────────────────

        /// Total serialised size of one slot entry in bytes.
        static constexpr std::size_t kSize = 8;

        /// Flag bit indicating the slot has been logically deleted.
        static constexpr std::uint16_t kFlagDeleted = 0x0001u;

        // ── Fields ────────────────────────────────────────────────────────────

        /// Byte offset of the tuple data from the start of the page body.
        std::uint16_t offset = 0;

        /// Length of the tuple data in bytes.
        std::uint16_t length = 0;

        /// Status flags (see @c kFlagDeleted).
        std::uint16_t flags = 0;

        /// Reserved; always zero.
        std::uint16_t reserved = 0;

        // ── Construction ──────────────────────────────────────────────────────

        TupleSlot() = default;

        /// Construct a live (non-deleted) slot with the given offset and length.
        TupleSlot(std::uint16_t offset, std::uint16_t length) noexcept;

        // ── Status predicates / mutators ──────────────────────────────────────

        /// Return true if the DELETED flag is set.
        [[nodiscard]] bool isDeleted() const noexcept;

        /// Set the DELETED flag.
        void markDeleted() noexcept;

        /// Clear the DELETED flag (re-activates the slot).
        void clearDeleted() noexcept;
    };

    static_assert(sizeof(TupleSlot) == TupleSlot::kSize,
                  "TupleSlot must be exactly 8 bytes");

} // namespace hamdb
