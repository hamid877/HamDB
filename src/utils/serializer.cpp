#include "utils/serializer.hpp"

#include <algorithm>
#include <cstring>

namespace hamdb
{

    // ── Construction ──────────────────────────────────────────────────────────

    Serializer::Serializer(std::span<std::byte> buffer) noexcept : buffer_(buffer), cursor_(0) {}

    // ── Private helper ────────────────────────────────────────────────────────

    Status Serializer::writeRaw(const std::uint8_t* src, std::size_t n) noexcept
    {
        if (n > buffer_.size() - cursor_)
        {
            return Status::IoError; // would overflow
        }

        // Copy byte-by-byte via std::byte — no reinterpret_cast, no memcpy in
        // the public-facing path.  The compiler elides this into a single store
        // instruction or SIMD move for common sizes.
        for (std::size_t i = 0; i < n; ++i)
        {
            buffer_[cursor_ + i] = static_cast<std::byte>(src[i]);
        }

        cursor_ += n;
        return Status::Ok;
    }

    // ── Scalar writers ────────────────────────────────────────────────────────

    Status Serializer::writeUInt8(std::uint8_t value) noexcept
    {
        const std::uint8_t bytes[1] = {value};
        return writeRaw(bytes, 1);
    }

    Status Serializer::writeUInt16(std::uint16_t value) noexcept
    {
        // Emit in little-endian order explicitly — no UB, no endian.h dependency
        const std::uint8_t bytes[2] = {
            static_cast<std::uint8_t>(value & 0xFFu),
            static_cast<std::uint8_t>((value >> 8u) & 0xFFu),
        };
        return writeRaw(bytes, 2);
    }

    Status Serializer::writeUInt32(std::uint32_t value) noexcept
    {
        const std::uint8_t bytes[4] = {
            static_cast<std::uint8_t>(value & 0xFFu),
            static_cast<std::uint8_t>((value >> 8u) & 0xFFu),
            static_cast<std::uint8_t>((value >> 16u) & 0xFFu),
            static_cast<std::uint8_t>((value >> 24u) & 0xFFu),
        };
        return writeRaw(bytes, 4);
    }

    Status Serializer::writeUInt64(std::uint64_t value) noexcept
    {
        const std::uint8_t bytes[8] = {
            static_cast<std::uint8_t>(value & 0xFFu),
            static_cast<std::uint8_t>((value >> 8u) & 0xFFu),
            static_cast<std::uint8_t>((value >> 16u) & 0xFFu),
            static_cast<std::uint8_t>((value >> 24u) & 0xFFu),
            static_cast<std::uint8_t>((value >> 32u) & 0xFFu),
            static_cast<std::uint8_t>((value >> 40u) & 0xFFu),
            static_cast<std::uint8_t>((value >> 48u) & 0xFFu),
            static_cast<std::uint8_t>((value >> 56u) & 0xFFu),
        };
        return writeRaw(bytes, 8);
    }

    Status Serializer::writeInt32(std::int32_t value) noexcept
    {
        // Reinterpret via unsigned — defined by C++20 two's-complement guarantee
        return writeUInt32(static_cast<std::uint32_t>(value));
    }

    Status Serializer::writeBool(bool value) noexcept
    {
        return writeUInt8(value ? std::uint8_t{1} : std::uint8_t{0});
    }

    // ── Composite writers ─────────────────────────────────────────────────────

    Status Serializer::writeBytes(std::span<const std::byte> data) noexcept
    {
        if (data.size() > buffer_.size() - cursor_)
        {
            return Status::IoError;
        }

        for (std::size_t i = 0; i < data.size(); ++i)
        {
            buffer_[cursor_ + i] = data[i];
        }

        cursor_ += data.size();
        return Status::Ok;
    }

    Status Serializer::writeUUID(const std::array<std::uint8_t, 16>& uuid) noexcept
    {
        return writeRaw(uuid.data(), 16);
    }

    Status Serializer::writeString(std::string_view value) noexcept
    {
        // Guard against strings longer than uint32_t can represent
        if (value.size() > UINT32_MAX)
        {
            return Status::InvalidArg;
        }

        const auto len = static_cast<std::uint32_t>(value.size());

        // Write 4-byte LE length prefix
        if (const Status s = writeUInt32(len); s != Status::Ok)
        {
            return s;
        }

        // Write raw UTF-8 bytes
        if (len == 0)
        {
            return Status::Ok;
        }

        // Reinterpret char* → uint8_t* — permitted by C++ aliasing rules for
        // unsigned char / uint8_t.
        const auto* src = reinterpret_cast<const std::uint8_t*>(value.data()); // NOLINT
        return writeRaw(src, len);
    }

    // ── Cursor utilities ──────────────────────────────────────────────────────

    std::size_t Serializer::position() const noexcept
    {
        return cursor_;
    }

    std::size_t Serializer::remaining() const noexcept
    {
        return buffer_.size() - cursor_;
    }

    void Serializer::reset() noexcept
    {
        cursor_ = 0;
    }

} // namespace hamdb
