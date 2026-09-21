#pragma once

#include "common/types.h"
#include "storage/page_header.h"
#include <array>
#include <cstddef>
#include <span>

namespace hamdb {

/**
 * @brief Fixed-size in-memory representation of a single database page.
 *
 * A @c Page is the fundamental unit of I/O in HamDB.  Its total on-disk size
 * is exactly @c kPageSize bytes (default 4 KiB).  The first @c PageHeader::kSize
 * bytes are reserved for the @c PageHeader; the remaining bytes form the
 * *body* available for data.
 *
 * @c Page owns its raw byte array and is non-copyable to prevent accidental
 * duplication of large fixed-size buffers.  Use @c std::move or hold pages
 * through smart pointers.
 *
 * @note Page is not thread-safe.  External synchronisation (e.g., latch) is
 *       the caller's responsibility.
 */
class Page {
public:
    /// Total size of the page in bytes (header + body).
    static constexpr std::size_t kSize = kPageSize;

    /// Number of bytes available for payload data after the header.
    static constexpr std::size_t kBodySize = kSize - PageHeader::kSize;

    // ── Construction / Destruction ────────────────────────────────────────────

    /// Construct a zeroed, uninitialised page.
    Page();

    /**
     * @brief Construct a page with a pre-populated header.
     *
     * The body is zero-initialised.
     *
     * @param header The header to install at offset 0.
     */
    explicit Page(const PageHeader& header);

    ~Page() = default;

    // Page is movable but not copyable (4 KiB stack/heap copy is expensive).
    Page(const Page&)            = delete;
    Page& operator=(const Page&) = delete;
    Page(Page&&)                 = default;
    Page& operator=(Page&&)      = default;

    // ── Header access ─────────────────────────────────────────────────────────

    /**
     * @brief Return a reference to the embedded page header.
     * @note  The header is always kept in sync with the raw byte buffer.
     */
    [[nodiscard]] PageHeader&       header();
    [[nodiscard]] const PageHeader& header() const;

    // ── Raw data access ───────────────────────────────────────────────────────

    /// Return a writable span over the entire page (header + body).
    [[nodiscard]] std::span<std::byte>       data();

    /// Return a read-only span over the entire page (header + body).
    [[nodiscard]] std::span<const std::byte> data() const;

    /// Return a writable span over the body (everything after the header).
    [[nodiscard]] std::span<std::byte>       body();

    /// Return a read-only span over the body.
    [[nodiscard]] std::span<const std::byte> body() const;

    // ── Utilities ─────────────────────────────────────────────────────────────

    /// Zero-fill the entire page, including the header.
    void clear();

    /// Return the logical page ID stored in the header.
    [[nodiscard]] PageId id() const;

private:
    /// Raw byte storage for the full page.
    std::array<std::byte, kSize> data_;

    /// In-memory mirror of the header fields (kept in sync with data_).
    PageHeader header_;
};

} // namespace hamdb
