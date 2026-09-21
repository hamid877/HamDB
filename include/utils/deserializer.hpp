#pragma once

/// @file deserializer.hpp
/// @brief Reads primitive and composite values from a raw byte buffer.

#include <cstdint>
#include <span>
#include <string>

namespace hamdb
{

    /**
     * @brief Reads primitive and composite values from a raw byte buffer.
     *
     * @c Deserializer is the counterpart to @c Serializer.  It wraps a read-only
     * byte span and advances an internal read cursor each time a value is
     * consumed.  It never allocates memory; the caller retains ownership of the
     * backing buffer.
     *
     * Example usage:
     * @code
     * std::span<const std::byte> data = ...; // data produced by Serializer
     * hamdb::Deserializer des(data);
     * std::uint32_t n = des.readUInt32();
     * std::string   s = des.readString();
     * @endcode
     *
     * @note Deserializer is not thread-safe.
     */
    class Deserializer
    {
    public:
        /// Construct a Deserializer over an existing read-only byte span.
        explicit Deserializer(std::span<const std::byte> buffer);

        /// @name Scalar readers
        /// @{
        [[nodiscard]] std::uint8_t readUInt8();
        [[nodiscard]] std::uint16_t readUInt16();
        [[nodiscard]] std::uint32_t readUInt32();
        [[nodiscard]] std::uint64_t readUInt64();
        [[nodiscard]] std::int8_t readInt8();
        [[nodiscard]] std::int16_t readInt16();
        [[nodiscard]] std::int32_t readInt32();
        [[nodiscard]] std::int64_t readInt64();
        [[nodiscard]] float readFloat();
        [[nodiscard]] double readDouble();
        [[nodiscard]] bool readBool();
        /// @}

        /// @name Composite readers
        /// @{
        /// Read a length-prefixed UTF-8 string (4-byte length header).
        [[nodiscard]] std::string readString();
        /// Read exactly @p length bytes and return a view into the backing buffer.
        [[nodiscard]] std::span<const std::byte> readBytes(std::size_t length);
        /// @}

        /// Return the number of bytes consumed so far.
        [[nodiscard]] std::size_t bytesRead() const;

        /// Return the number of bytes remaining in the backing buffer.
        [[nodiscard]] std::size_t bytesRemaining() const;

        /// Reset the read cursor to the beginning of the buffer.
        void reset();

    private:
        std::span<const std::byte> buffer_; ///< Backing storage (non-owning).
        std::size_t cursor_;                ///< Current read position (bytes).
    };

} // namespace hamdb
