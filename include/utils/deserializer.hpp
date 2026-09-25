#pragma once

/// @file deserializer.hpp
/// @brief Reads primitive and composite values from a caller-owned byte buffer.
///
/// @c Deserializer is the exact counterpart to @c Serializer.  It wraps a
/// read-only @c std::span<const std::byte> and advances an internal read cursor
/// on each successful read.  Every method returns a @c Status; on overflow the
/// out-parameter is left unchanged.
///
/// Decoding contract:
///   - All multi-byte integers are read **little-endian**.
///   - Strings are decoded from a 4-byte LE length prefix followed by raw bytes.
///   - UUIDs are read as exactly 16 raw bytes.
///   - No dynamic allocation is performed (string reads do allocate std::string).
///   - Cursor does not advance on failure.

#include "common/enums.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace hamdb
{

    /**
     * @brief Zero-allocation (except @c readString), bounds-checked binary deserializer.
     *
     * The caller supplies the backing buffer at construction time and owns its
     * lifetime.  @c Deserializer never frees memory, and only allocates for the
     * returned @c std::string in @c readString().
     *
     * @note Not thread-safe.
     */
    class Deserializer
    {
    public:
        // ── Construction ──────────────────────────────────────────────────────

        /**
         * @brief Construct a Deserializer over a caller-owned read-only byte span.
         *
         * @param buffer Read-only memory region to consume.  Must outlive this object.
         */
        explicit Deserializer(std::span<const std::byte> buffer) noexcept;

        // ── Scalar readers ────────────────────────────────────────────────────

        /**
         * @brief Read a single unsigned byte.
         * @param[out] out Receives the value on success.
         * @return @c Status::Ok or @c Status::IoError on underflow.
         */
        [[nodiscard]] Status readUInt8(std::uint8_t& out) noexcept;

        /**
         * @brief Read a 16-bit unsigned integer (little-endian).
         * @param[out] out Receives the value on success.
         * @return @c Status::Ok or @c Status::IoError.
         */
        [[nodiscard]] Status readUInt16(std::uint16_t& out) noexcept;

        /**
         * @brief Read a 32-bit unsigned integer (little-endian).
         * @param[out] out Receives the value on success.
         * @return @c Status::Ok or @c Status::IoError.
         */
        [[nodiscard]] Status readUInt32(std::uint32_t& out) noexcept;

        /**
         * @brief Read a 64-bit unsigned integer (little-endian).
         * @param[out] out Receives the value on success.
         * @return @c Status::Ok or @c Status::IoError.
         */
        [[nodiscard]] Status readUInt64(std::uint64_t& out) noexcept;

        /**
         * @brief Read a 32-bit signed integer (little-endian, two's complement).
         * @param[out] out Receives the value on success.
         * @return @c Status::Ok or @c Status::IoError.
         */
        [[nodiscard]] Status readInt32(std::int32_t& out) noexcept;

        /**
         * @brief Read a boolean byte (0x00 → false, anything else → true).
         * @param[out] out Receives the value on success.
         * @return @c Status::Ok or @c Status::IoError.
         */
        [[nodiscard]] Status readBool(bool& out) noexcept;

        // ── Composite readers ─────────────────────────────────────────────────

        /**
         * @brief Read exactly @p length bytes into a non-owning span.
         *
         * The returned span is a view into the backing buffer — it is valid
         * only while the buffer lives.  On failure @p out is unchanged.
         *
         * @param length   Number of bytes to read.
         * @param[out] out View into the backing buffer on success.
         * @return @c Status::Ok or @c Status::IoError.
         */
        [[nodiscard]] Status readBytes(std::size_t length,
                                       std::span<const std::byte>& out) noexcept;

        /**
         * @brief Read a 128-bit UUID (exactly 16 raw bytes).
         * @param[out] out Receives the UUID on success.
         * @return @c Status::Ok or @c Status::IoError.
         */
        [[nodiscard]] Status readUUID(std::array<std::uint8_t, 16>& out) noexcept;

        /**
         * @brief Read a length-prefixed UTF-8 string.
         *
         * Decoding: 4-byte LE length then raw bytes.  Allocates a @c std::string.
         * On failure @p out is unchanged.
         *
         * @param[out] out Receives the decoded string on success.
         * @return @c Status::Ok or @c Status::IoError.
         */
        [[nodiscard]] Status readString(std::string& out);

        // ── Cursor utilities ──────────────────────────────────────────────────

        /// Return the number of bytes consumed so far.
        [[nodiscard]] std::size_t position() const noexcept;

        /// Return the number of bytes remaining in the backing buffer.
        [[nodiscard]] std::size_t remaining() const noexcept;

        /// Reset the read cursor to position 0.
        void reset() noexcept;

    private:
        std::span<const std::byte> buffer_; ///< Caller-owned backing storage.
        std::size_t cursor_;                ///< Current read position (bytes from start).

        /// Copy exactly @p n bytes from the buffer at @c cursor_ into @p dst,
        /// advancing @c cursor_ by @p n.  Returns @c Status::IoError on underflow.
        [[nodiscard]] Status readRaw(std::uint8_t* dst, std::size_t n) noexcept;
    };

} // namespace hamdb
