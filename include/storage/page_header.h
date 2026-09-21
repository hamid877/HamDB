#pragma once

#include "common/types.h"
#include <cstdint>
#include <cstring>
#include <array>

namespace hamdb {

/**
 * @brief Fixed-size header stored at the beginning of every database page.
 *
 * @c PageHeader occupies the first bytes of each @c Page and holds bookkeeping
 * data that the storage engine needs to manage the page without reading its
 * full contents.
 *
 * Layout (all fields are little-endian on disk):
 * | Offset | Size | Field           |
 * |--------|------|-----------------|
 * |      0 |    4 | page_id         |
 * |      4 |    1 | page_type       |
 * |      5 |    2 | free_space_ptr  |
 * |      7 |    2 | slot_count      |
 * |      9 |    4 | checksum        |
 * |     13 |    3 | (reserved)      |
 *
 * Total size: 16 bytes.
 */
struct PageHeader {
    /// Size of the serialised header in bytes.
    static constexpr std::size_t kSize = 16;

    /// Identifies the kind of data stored in a page.
    enum class PageType : std::uint8_t {
        Invalid  = 0,  ///< Not yet initialised.
        Data     = 1,  ///< Stores heap-file tuples.
        Internal = 2,  ///< B-tree internal node (future use).
        Leaf     = 3,  ///< B-tree leaf node (future use).
        Meta     = 4,  ///< Database-level metadata.
    };

    PageId      page_id        = kInvalidPageId;  ///< Logical page number.
    PageType    page_type      = PageType::Invalid; ///< Content classification.
    std::uint16_t free_space_ptr = 0;             ///< Byte offset to first free byte.
    std::uint16_t slot_count    = 0;              ///< Number of slot-directory entries.
    std::uint32_t checksum      = 0;              ///< CRC-32 of the page body.

    // ── Construction ──────────────────────────────────────────────────────────

    /// Default-construct an uninitialised header.
    PageHeader() = default;

    /**
     * @brief Construct a header for a freshly allocated page.
     *
     * @param id   The logical page identifier.
     * @param type The intended content type for the page.
     */
    PageHeader(PageId id, PageType type);

    // ── Comparison ────────────────────────────────────────────────────────────

    /// Equality operator — compares all fields.
    bool operator==(const PageHeader& other) const noexcept;
    bool operator!=(const PageHeader& other) const noexcept;
};

} // namespace hamdb
