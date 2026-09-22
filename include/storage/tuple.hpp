#pragma once

/// @file tuple.hpp
/// @brief Lightweight abstraction representing a single database row.

#include <cstddef>
#include <span>
#include <vector>

namespace hamdb
{

    /**
     * @brief A generic, schema-less wrapper for serialized tuple data.
     *
     * @c Tuple owns its raw byte payload. It replaces the use of raw
     * @c std::span<const std::byte> to provide strong typing and safe lifetime
     * management when tuples are passed around the storage layer.
     */
    class Tuple
    {
    public:
        // ── Construction ──────────────────────────────────────────────────────────

        /// Default construct an empty tuple.
        Tuple() = default;

        /**
         * @brief Construct a tuple by copying the given byte span.
         * @param data The serialized tuple payload.
         */
        explicit Tuple(std::span<const std::byte> data);

        // Movable, copyable.
        Tuple(const Tuple&) = default;
        Tuple& operator=(const Tuple&) = default;
        Tuple(Tuple&&) noexcept = default;
        Tuple& operator=(Tuple&&) noexcept = default;

        // ── Accessors ─────────────────────────────────────────────────────────────

        /// Return a read-only view of the tuple's payload.
        [[nodiscard]] std::span<const std::byte> data() const noexcept;

        /// Return the size of the payload in bytes.
        [[nodiscard]] std::size_t size() const noexcept;

        /// Return true if the tuple payload is empty.
        [[nodiscard]] bool empty() const noexcept;

    private:
        std::vector<std::byte> data_; ///< Owned serialized payload.
    };

} // namespace hamdb
