#include "utils/serializer.hpp"

namespace hamdb
{

    Serializer::Serializer(std::span<std::byte> buffer) : buffer_(buffer), cursor_(0) {}

    void Serializer::writeUInt8([[maybe_unused]] std::uint8_t value) {}
    void Serializer::writeUInt16([[maybe_unused]] std::uint16_t value) {}
    void Serializer::writeUInt32([[maybe_unused]] std::uint32_t value) {}
    void Serializer::writeUInt64([[maybe_unused]] std::uint64_t value) {}
    void Serializer::writeInt8([[maybe_unused]] std::int8_t value) {}
    void Serializer::writeInt16([[maybe_unused]] std::int16_t value) {}
    void Serializer::writeInt32([[maybe_unused]] std::int32_t value) {}
    void Serializer::writeInt64([[maybe_unused]] std::int64_t value) {}
    void Serializer::writeFloat([[maybe_unused]] float value) {}
    void Serializer::writeDouble([[maybe_unused]] double value) {}
    void Serializer::writeBool([[maybe_unused]] bool value) {}
    void Serializer::writeString([[maybe_unused]] const std::string& value) {}
    void Serializer::writeBytes([[maybe_unused]] std::span<const std::byte> data) {}

    std::size_t Serializer::bytesWritten() const
    {
        return cursor_;
    }

    std::size_t Serializer::bytesRemaining() const
    {
        return buffer_.size() - cursor_;
    }

    void Serializer::reset()
    {
        cursor_ = 0;
    }

} // namespace hamdb
