#include "utils/deserializer.hpp"

#include <cstring>

namespace hamdb
{

    // ── Construction ──────────────────────────────────────────────────────────

    Deserializer::Deserializer(std::span<const std::byte> buffer) noexcept
        : buffer_(buffer), cursor_(0)
    {
    }

    // ── Private helper ────────────────────────────────────────────────────────

    Status Deserializer::readRaw(std::uint8_t* dst, std::size_t n) noexcept
    {
        if (n > buffer_.size() - cursor_)
        {
            return Status::IoError; // would underflow
        }

        for (std::size_t i = 0; i < n; ++i)
        {
            dst[i] = static_cast<std::uint8_t>(buffer_[cursor_ + i]);
        }

        cursor_ += n;
        return Status::Ok;
    }

    // ── Scalar readers ────────────────────────────────────────────────────────

    Status Deserializer::readUInt8(std::uint8_t& out) noexcept
    {
        std::uint8_t bytes[1] = {};
        if (const Status s = readRaw(bytes, 1); s != Status::Ok)
        {
            return s;
        }
        out = bytes[0];
        return Status::Ok;
    }

    Status Deserializer::readUInt16(std::uint16_t& out) noexcept
    {
        std::uint8_t bytes[2] = {};
        if (const Status s = readRaw(bytes, 2); s != Status::Ok)
        {
            return s;
        }
        out = static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[0]) |
                                         (static_cast<std::uint16_t>(bytes[1]) << 8u));
        return Status::Ok;
    }

    Status Deserializer::readUInt32(std::uint32_t& out) noexcept
    {
        std::uint8_t bytes[4] = {};
        if (const Status s = readRaw(bytes, 4); s != Status::Ok)
        {
            return s;
        }
        out = (static_cast<std::uint32_t>(bytes[0])) |
              (static_cast<std::uint32_t>(bytes[1]) << 8u) |
              (static_cast<std::uint32_t>(bytes[2]) << 16u) |
              (static_cast<std::uint32_t>(bytes[3]) << 24u);
        return Status::Ok;
    }

    Status Deserializer::readUInt64(std::uint64_t& out) noexcept
    {
        std::uint8_t bytes[8] = {};
        if (const Status s = readRaw(bytes, 8); s != Status::Ok)
        {
            return s;
        }
        out = (static_cast<std::uint64_t>(bytes[0])) |
              (static_cast<std::uint64_t>(bytes[1]) << 8u) |
              (static_cast<std::uint64_t>(bytes[2]) << 16u) |
              (static_cast<std::uint64_t>(bytes[3]) << 24u) |
              (static_cast<std::uint64_t>(bytes[4]) << 32u) |
              (static_cast<std::uint64_t>(bytes[5]) << 40u) |
              (static_cast<std::uint64_t>(bytes[6]) << 48u) |
              (static_cast<std::uint64_t>(bytes[7]) << 56u);
        return Status::Ok;
    }

    Status Deserializer::readInt32(std::int32_t& out) noexcept
    {
        std::uint32_t raw = 0;
        if (const Status s = readUInt32(raw); s != Status::Ok)
        {
            return s;
        }
        // C++20 guarantees two's-complement — this static_cast is well-defined.
        out = static_cast<std::int32_t>(raw);
        return Status::Ok;
    }

    Status Deserializer::readBool(bool& out) noexcept
    {
        std::uint8_t byte = 0;
        if (const Status s = readUInt8(byte); s != Status::Ok)
        {
            return s;
        }
        out = (byte != 0u);
        return Status::Ok;
    }

    // ── Composite readers ─────────────────────────────────────────────────────

    Status Deserializer::readBytes(std::size_t length, std::span<const std::byte>& out) noexcept
    {
        if (length > buffer_.size() - cursor_)
        {
            return Status::IoError;
        }
        out = buffer_.subspan(cursor_, length);
        cursor_ += length;
        return Status::Ok;
    }

    Status Deserializer::readUUID(std::array<std::uint8_t, 16>& out) noexcept
    {
        return readRaw(out.data(), 16);
    }

    Status Deserializer::readString(std::string& out)
    {
        // Read 4-byte LE length prefix
        std::uint32_t len = 0;
        if (const Status s = readUInt32(len); s != Status::Ok)
        {
            return s;
        }

        // Bounds-check before allocating
        if (len > buffer_.size() - cursor_)
        {
            // Undo the length read so the cursor stays consistent on failure
            cursor_ -= 4;
            return Status::IoError;
        }

        // Build the string from raw bytes in the buffer
        std::string result(len, '\0');
        for (std::uint32_t i = 0; i < len; ++i)
        {
            result[i] = static_cast<char>(static_cast<std::uint8_t>(buffer_[cursor_ + i]));
        }

        cursor_ += len;
        out = std::move(result);
        return Status::Ok;
    }

    // ── Cursor utilities ──────────────────────────────────────────────────────

    std::size_t Deserializer::position() const noexcept
    {
        return cursor_;
    }

    std::size_t Deserializer::remaining() const noexcept
    {
        return buffer_.size() - cursor_;
    }

    void Deserializer::reset() noexcept
    {
        cursor_ = 0;
    }

} // namespace hamdb
