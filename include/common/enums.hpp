#pragma once

/// @file enums.hpp
/// @brief Engine-wide enumeration types for HamDB.
///
/// This header collects all shared enumerations in one place so that
/// individual modules never need to depend on each other just to name a type.

#include <cstdint>

namespace hamdb
{

    /// @defgroup enums Shared Enumerations
    /// @{

    // ── Page classification ───────────────────────────────────────────────────────

    /**
     * @brief Identifies the kind of data stored in a database page.
     *
     * The value is persisted in the @c PageHeader on disk, so assigned integers
     * must never be reused or reordered between format versions.
     */
    enum class PageType : std::uint8_t
    {
        Free = 0,          ///< Page is unallocated and available for reuse.
        Metadata = 1,      ///< Database-level metadata (file header, catalog root).
        Table = 2,         ///< Heap-file page storing row data for a table.
        Index = 3,         ///< B-tree node (internal or leaf) for an index.
        Overflow = 4,      ///< Overflow page for variable-length column values.
        BTreeInternal = 5, ///< B+ Tree internal (routing) node page.
        BTreeLeaf = 6,     ///< B+ Tree leaf node page holding key/value pairs.
    };

    // ── Operation results ─────────────────────────────────────────────────────────

    /**
     * @brief Enumeration of result codes returned by HamDB operations.
     *
     * Functions that can fail return a @c Status value instead of throwing
     * exceptions, keeping the error path explicit and zero-cost on the happy path.
     */
    enum class Status : std::uint8_t
    {
        Ok = 0,        ///< Operation completed successfully.
        NotFound,      ///< Requested resource does not exist.
        InvalidArg,    ///< One or more arguments are invalid.
        IoError,       ///< An I/O error occurred during a disk operation.
        OutOfMemory,   ///< Memory allocation failed.
        Corruption,    ///< On-disk data is corrupt or inconsistent.
        NotSupported,  ///< Operation is not yet implemented.
        AlreadyExists, ///< Resource already exists and cannot be created again.
        BufferPoolFull,///< Buffer pool has no free frames available.
        PageFull,      ///< Page has no free space.
        Unknown,       ///< An unclassified internal error occurred.
    };

    /**
     * @brief Convert a @c Status code to a human-readable string.
     *
     * @param status The status code to describe.
     * @return A descriptive string literal for the status.
     */
    [[nodiscard]] const char* statusToString(Status status) noexcept;

    /// @}

} // namespace hamdb
