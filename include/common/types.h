#pragma once

#include <cstdint>
#include <string>

namespace hamdb {

/// @defgroup common Common Utilities
/// @{

// ── Fundamental numeric types ─────────────────────────────────────────────────

/// Identifies a page by its zero-based position within a database file.
using PageId = std::uint32_t;

/// Size of a single database page in bytes (default: 4 KiB).
inline constexpr std::size_t kPageSize = 4096;

/// The maximum number of pages that can exist in a single database file.
inline constexpr PageId kMaxPageId = UINT32_MAX - 1;

/// Sentinel value representing an invalid or null page ID.
inline constexpr PageId kInvalidPageId = UINT32_MAX;

// ── Status codes ──────────────────────────────────────────────────────────────

/**
 * @brief Enumeration of result codes returned by HamDB operations.
 *
 * Functions that can fail return a @c Status value instead of throwing
 * exceptions, keeping the error path explicit and zero-cost on the happy path.
 */
enum class Status : std::uint8_t {
    Ok = 0,         ///< Operation completed successfully.
    NotFound,       ///< Requested resource does not exist.
    InvalidArg,     ///< One or more arguments are invalid.
    IoError,        ///< An I/O error occurred during a disk operation.
    OutOfMemory,    ///< Memory allocation failed.
    Corruption,     ///< On-disk data is corrupt or inconsistent.
    NotSupported,   ///< Operation is not yet implemented.
    AlreadyExists,  ///< Resource already exists and cannot be created again.
    Unknown,        ///< An unclassified internal error occurred.
};

/**
 * @brief Convert a @c Status code to a human-readable string.
 *
 * @param status The status code to describe.
 * @return A non-owning string-view-compatible literal for the status.
 */
[[nodiscard]] std::string statusToString(Status status);

/// @}

} // namespace hamdb
