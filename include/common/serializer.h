#pragma once

#include <span>
#include <cstdint>
#include <string>

namespace hamdb {

/**
 * @brief Writes primitive and composite values into a raw byte buffer.
 *
 * @c Serializer wraps a caller-provided byte span and advances an internal
 * write cursor each time a value is appended.  It never allocates memory
 * of its own; the caller retains ownership of the backing buffer.
 *
 * Example usage:
 * @code
 * std::array<std::byte, 64> buf{};
 * hamdb::Serializer ser(buf);
 * ser.writeUInt32(42);
 * ser.writeString("hello");
 * @endcode
 *
 * @note Serializer is not thread-safe.
 */
class Serializer {
public:
    /// Construct a Serializer over an existing writable byte span.
    explicit Serializer(std::span<std::byte> buffer);

    /// @name Scalar writers
    /// @{
    void writeUInt8(std::uint8_t value);
    void writeUInt16(std::uint16_t value);
    void writeUInt32(std::uint32_t value);
    void writeUInt64(std::uint64_t value);
    void writeInt8(std::int8_t value);
    void writeInt16(std::int16_t value);
    void writeInt32(std::int32_t value);
    void writeInt64(std::int64_t value);
    void writeFloat(float value);
    void writeDouble(double value);
    void writeBool(bool value);
    /// @}

    /// @name Composite writers
    /// @{
    /// Write a length-prefixed UTF-8 string (4-byte length header).
    void writeString(const std::string& value);
    /// Write raw bytes without any length prefix.
    void writeBytes(std::span<const std::byte> data);
    /// @}

    /// Return the number of bytes written so far.
    [[nodiscard]] std::size_t bytesWritten() const;

    /// Return the number of bytes remaining in the backing buffer.
    [[nodiscard]] std::size_t bytesRemaining() const;

    /// Reset the write cursor to the beginning of the buffer.
    void reset();

private:
    std::span<std::byte> buffer_;  ///< Backing storage (non-owning).
    std::size_t          cursor_;  ///< Current write position (bytes).
};

} // namespace hamdb
