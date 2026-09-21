#include "utils/deserializer.hpp"

namespace hamdb
{

    Deserializer::Deserializer(std::span<const std::byte> buffer) : buffer_(buffer), cursor_(0) {}

    std::uint8_t Deserializer::readUInt8()
    {
        return {};
    }
    std::uint16_t Deserializer::readUInt16()
    {
        return {};
    }
    std::uint32_t Deserializer::readUInt32()
    {
        return {};
    }
    std::uint64_t Deserializer::readUInt64()
    {
        return {};
    }
    std::int8_t Deserializer::readInt8()
    {
        return {};
    }
    std::int16_t Deserializer::readInt16()
    {
        return {};
    }
    std::int32_t Deserializer::readInt32()
    {
        return {};
    }
    std::int64_t Deserializer::readInt64()
    {
        return {};
    }
    float Deserializer::readFloat()
    {
        return {};
    }
    double Deserializer::readDouble()
    {
        return {};
    }
    bool Deserializer::readBool()
    {
        return {};
    }
    std::string Deserializer::readString()
    {
        return {};
    }

    std::span<const std::byte> Deserializer::readBytes([[maybe_unused]] std::size_t length)
    {
        return {};
    }

    std::size_t Deserializer::bytesRead() const
    {
        return cursor_;
    }

    std::size_t Deserializer::bytesRemaining() const
    {
        return buffer_.size() - cursor_;
    }

    void Deserializer::reset()
    {
        cursor_ = 0;
    }

} // namespace hamdb
