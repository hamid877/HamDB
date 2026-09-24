#pragma once

/// @file constants.hpp
/// @brief Compile-time constants for the HamDB storage engine.
///
/// All values in this file are `inline constexpr` and produce no object code.
/// Consumers only need to include this header — no linking required.

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hamdb
{

    /// @defgroup constants Compile-time Constants
    /// @{

    // ── Magic & versioning ────────────────────────────────────────────────────────

    /// Eight-byte ASCII magic written at offset 0 of every .hamdb file.
    /// Must be exactly 8 characters — no NUL terminator is stored on disk.
    inline constexpr std::string_view kDbMagic = "HAMDB001";

    /// Short human-readable identifier (4 chars) used in log/display output.
    inline constexpr std::string_view kMagic = "HMDB";

    /// On-disk format version.  Increment when the page layout changes
    /// in a backwards-incompatible way.
    inline constexpr std::uint16_t kFormatVersion = 1;

    // ── Page geometry ─────────────────────────────────────────────────────────────

    /// Size of a single database page in bytes (default: 4 KiB).
    inline constexpr std::size_t kPageSize = 4096;

    /// Size of the on-disk page header in bytes.
    /// Must match @c PageHeader::kSize.
    inline constexpr std::size_t kPageHeaderSize = 24;

    /// Number of payload bytes available in a page after the header.
    inline constexpr std::size_t kPageBodySize = kPageSize - kPageHeaderSize;

    // ── Page ID sentinels ─────────────────────────────────────────────────────────

    /// Type alias for a logical page number within a database file.
    using PageId = std::uint32_t;

    /// The largest valid page ID.
    inline constexpr PageId kMaxPageId = UINT32_MAX - 1;

    /// Sentinel value representing an invalid or null page ID.
    inline constexpr PageId kInvalidPageId = UINT32_MAX;

    // ── Default filenames ─────────────────────────────────────────────────────────

    /// Default extension for HamDB database files.
    inline constexpr std::string_view kDefaultFileExtension = ".hamdb";

    /// Default name used when creating a database without an explicit filename.
    inline constexpr std::string_view kDefaultDatabaseName = "default.hamdb";

    /// @}

} // namespace hamdb
