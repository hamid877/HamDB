#pragma once

/// @file database_metadata.hpp
/// @brief On-disk metadata structure occupying the first 64 bytes of page 0.
///
/// @c DatabaseMetadata is the fixed-size record that HamDB writes at the very
/// beginning of every `.hamdb` file.  It is always serialised into the first
/// 64 bytes of page 0, leaving the remaining 4032 bytes of that page available
/// for future catalog data.
///
/// Layout (all integers are little-endian on disk):
/// | Offset | Size | Field            | Description                        |
/// |--------|------|------------------|------------------------------------|
/// |      0 |    8 | magic            | ASCII "HAMDB001" (no NUL)          |
/// |      8 |    2 | version          | Format version (currently 1)       |
/// |     10 |    2 | (reserved)       | Padding, must be zero              |
/// |     12 |    4 | page_size        | Page size in bytes (default 4096)  |
/// |     16 |    8 | page_count       | Total pages in the file            |
/// |     24 |    4 | free_page_ptr    | First free-list page (INVALID=none)|
/// |     28 |    4 | (reserved)       | Padding, must be zero              |
/// |     32 |   16 | uuid             | Random 128-bit database identifier |
/// |     48 |    8 | created_at       | Unix timestamp of creation (UTC)   |
/// |     56 |    8 | (reserved)       | Reserved for future use            |
///
/// Total: 64 bytes.

#include "common/constants.hpp"
#include <array>
#include <cstdint>
#include <cstring>

namespace hamdb
{

    /**
     * @brief Serialised header stored in the first 64 bytes of page 0.
     *
     * All write operations use `std::memcpy` to avoid undefined behaviour from
     * unaligned pointer casts.  The struct itself is not `#pragma pack`-ed;
     * callers must use @c serialize() / @c deserialize() to move data on/off disk.
     */
    struct DatabaseMetadata
    {
        /// Total size of the serialised metadata record in bytes.
        static constexpr std::size_t kSize = 64;

        /// Eight-byte ASCII magic string that identifies a HamDB file.
        static constexpr std::string_view kMagicValue = kDbMagic;

        // ── Fields ────────────────────────────────────────────────────────────

        /// Magic bytes (must equal @c kMagicValue).
        std::array<char, 8> magic{};

        /// On-disk format version.
        std::uint16_t version = 0;

        /// Reserved padding (must be zero).
        std::uint16_t reserved0 = 0;

        /// Page size used when the database was created.
        std::uint32_t page_size = 0;

        /// Total number of pages currently in the file.
        std::uint64_t page_count = 0;

        /// Page ID of the first free-list page; @c kInvalidPageId if none.
        std::uint32_t free_page_ptr = kInvalidPageId;

        /// Reserved padding (must be zero).
        std::uint32_t reserved1 = 0;

        /// Random 128-bit identifier assigned at creation time.
        std::array<std::uint8_t, 16> uuid{};

        /// Unix timestamp (seconds since epoch) recorded at creation.
        std::uint64_t created_at = 0;

        /// Reserved for future use.
        std::uint64_t reserved2 = 0;

        // ── Helpers ───────────────────────────────────────────────────────────

        /**
         * @brief Serialize this struct into a 64-byte output buffer.
         *
         * @param out Destination buffer of at least @c kSize bytes.
         */
        void serialize(std::array<std::uint8_t, kSize>& out) const noexcept;

        /**
         * @brief Populate this struct from a 64-byte input buffer.
         *
         * @param in Source buffer of at least @c kSize bytes.
         */
        void deserialize(const std::array<std::uint8_t, kSize>& in) noexcept;

        /// Return true if the magic field equals @c kMagicValue.
        [[nodiscard]] bool hasMagic() const noexcept;

        /// Return true if @c version equals @c kFormatVersion.
        [[nodiscard]] bool hasValidVersion() const noexcept;
    };

} // namespace hamdb
