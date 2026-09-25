#pragma once

/// @file serializer.hpp
/// @brief Writes primitive and composite values into a caller-owned byte buffer.
///
/// @c Serializer wraps a @c std::span<std::byte> and advances an internal write
/// cursor each time a value is appended.  Every write method returns a
/// @c Status so callers can distinguish overflow from logic errors without
/// exceptions.
///
/// Encoding contract:
///   - All multi-byte integers are written **little-endian**.
///   - Strings are length-prefixed with a @c uint32_t (4 bytes, LE) followed
///     by the raw UTF-8 bytes (no NUL terminator).
///   - UUIDs are written as exactly 16 raw bytes with no framing.
///   - No dynamic allocation is ever performed inside this class.
///   - No @c reinterpret_cast appears in the public API.

#include "common/enums.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace hamdb
{

    /**
     * @brief Zero-allocation, bounds-checked binary serializer.
     *
     * The caller supplies the backing buffer at construction time and owns its
     * lifetime.  @c Serializer never allocates or frees memory.
     *
     * @note Not thread-safe.
     */
    class Serializer
    {
    public:
        // ── Construction ──────────────────────────────────────────────────────

        /**
         * @brief Construct a Serializer over a caller-owned byte span.
         *
         * @param buffer Writable memory region to write into.  Must outlive
         *               this object.
         */
        explicit Serializer(std::span<std::byte> buffer) noexcept;

        // ── Scalar writers ────────────────────────────────────────────────────

        /**
         * @brief Write a single unsigned byte.
         * @return @c Status::Ok, or @c Status::IoError if the buffer is full.
         */
        [[nodiscard]] Status writeUInt8(std::uint8_t value) noexcept;

        /**
         * @brief Write a 16-bit unsigned integer (little-endian).
         * @return @c Status::Ok or @c Status::IoError.
         */
        [[nodiscard]] Status writeUInt16(std::uint16_t value) noexcept;

        /**
         * @brief Write a 32-bit unsigned integer (little-endian).
         * @return @c Status::Ok or @c Status::IoError.
         */
        [[nodiscard]] Status writeUInt32(std::uint32_t value) noexcept;

        /**
         * @brief Write a 64-bit unsigned integer (little-endian).
         * @return @c Status::Ok or @c Status::IoError.
         */
        [[nodiscard]] Status writeUInt64(std::uint64_t value) noexcept;

        /**
         * @brief Write a 32-bit signed integer (little-endian, two's complement).
         * @return @c Status::Ok or @c Status::IoError.
         */
        [[nodiscard]] Status writeInt32(std::int32_t value) noexcept;

        /**
         * @brief Write a boolean as a single byte (0x00 = false, 0x01 = true).
         * @return @c Status::Ok or @c Status::IoError.
         */
        [[nodiscard]] Status writeBool(bool value) noexcept;

        // ── Composite writers ─────────────────────────────────────────────────

        /**
         * @brief Write @p data.size() raw bytes without any length prefix.
         *
         * @param data Source bytes.
         * @return @c Status::Ok or @c Status::IoError if insufficient space.
         */
        [[nodiscard]] Status writeBytes(std::span<const std::byte> data) noexcept;

        /**
         * @brief Write a 128-bit UUID as exactly 16 raw bytes.
         *
         * @param uuid Source UUID byte array.
         * @return @c Status::Ok or @c Status::IoError.
         */
        [[nodiscard]] Status writeUUID(const std::array<std::uint8_t, 16>& uuid) noexcept;

        /**
         * @brief Write a length-prefixed UTF-8 string.
         *
         * Encoding: 4-byte little-endian length followed by the raw bytes of
         * @p value.  No NUL terminator is written.
         *
         * @param value String to write (may be empty).
         * @return @c Status::Ok or @c Status::IoError if insufficient space.
         */
        [[nodiscard]] Status writeString(std::string_view value) noexcept;

        // ── Cursor utilities ──────────────────────────────────────────────────

        /// Return the number of bytes written so far.
        [[nodiscard]] std::size_t position() const noexcept;

        /// Return the number of bytes remaining in the backing buffer.
        [[nodiscard]] std::size_t remaining() const noexcept;

        /// Reset the write cursor to position 0 (does not clear the buffer).
        void reset() noexcept;

    private:
        std::span<std::byte> buffer_; ///< Caller-owned backing storage.
        std::size_t cursor_;          ///< Current write position (bytes from start).

        /// Write exactly @p n bytes from @p src into the buffer at @c cursor_,
        /// advancing @c cursor_ by @p n.  Returns @c Status::IoError if there
        /// is insufficient space.
        [[nodiscard]] Status writeRaw(const std::uint8_t* src, std::size_t n) noexcept;
    };

} // namespace hamdb
