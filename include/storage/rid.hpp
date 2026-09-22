#pragma once

/// @file rid.hpp
/// @brief Record Identifier, uniquely locating a tuple in the database.

#include "common/constants.hpp"
#include <cstdint>

namespace hamdb
{

    /**
     * @brief Record Identifier (RID) uniquely locates a tuple.
     *
     * A RID consists of the logical page number containing the tuple,
     * and the slot index within that page's slot directory.
     */
    class RID
    {
    public:
        // ── Construction ──────────────────────────────────────────────────────────

        /// Default construct an invalid RID.
        RID() = default;

        /**
         * @brief Construct a valid RID.
         * @param page_id The logical page number.
         * @param slot_id The slot index within the page.
         */
        RID(PageId page_id, std::uint16_t slot_id);

        // ── Accessors ─────────────────────────────────────────────────────────────

        [[nodiscard]] PageId getPageId() const noexcept;
        [[nodiscard]] std::uint16_t getSlotId() const noexcept;

        /// Return true if this RID points to a valid page (i.e. not kInvalidPageId).
        [[nodiscard]] bool isValid() const noexcept;

        // ── Comparison Operators ──────────────────────────────────────────────────

        bool operator==(const RID& other) const noexcept;
        bool operator!=(const RID& other) const noexcept;
        bool operator<(const RID& other) const noexcept;
        bool operator>(const RID& other) const noexcept;
        bool operator<=(const RID& other) const noexcept;
        bool operator>=(const RID& other) const noexcept;

    private:
        PageId page_id_ = kInvalidPageId;
        std::uint16_t slot_id_ = 0;
    };

    /// Represents an invalid record identifier.
    constexpr RID kInvalidRID{};

} // namespace hamdb
