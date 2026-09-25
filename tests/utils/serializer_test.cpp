#include "common/enums.hpp"
#include "utils/deserializer.hpp"
#include "utils/serializer.hpp"
#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <span>
#include <string>

namespace hamdb
{

    // ─────────────────────────────────────────────────────────────────────────
    // Helpers
    // ─────────────────────────────────────────────────────────────────────────

    /// Create a fixed-size buffer, return a Serializer over it.
    template <std::size_t N> static Serializer makeSer(std::array<std::byte, N>& buf)
    {
        return Serializer(std::span<std::byte>(buf));
    }

    template <std::size_t N> static Deserializer makeDes(const std::array<std::byte, N>& buf)
    {
        return Deserializer(std::span<const std::byte>(buf));
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Serializer — basic construction
    // ─────────────────────────────────────────────────────────────────────────

    TEST(SerializerTest, InitialPositionIsZero)
    {
        std::array<std::byte, 16> buf{};
        auto ser = makeSer(buf);
        EXPECT_EQ(ser.position(), 0u);
    }

    TEST(SerializerTest, InitialRemainingEqualsBufferSize)
    {
        std::array<std::byte, 16> buf{};
        auto ser = makeSer(buf);
        EXPECT_EQ(ser.remaining(), 16u);
    }

    TEST(SerializerTest, ResetRestoresCursor)
    {
        std::array<std::byte, 8> buf{};
        auto ser = makeSer(buf);
        ASSERT_EQ(ser.writeUInt32(0xDEADBEEFu), Status::Ok);
        EXPECT_EQ(ser.position(), 4u);
        ser.reset();
        EXPECT_EQ(ser.position(), 0u);
        EXPECT_EQ(ser.remaining(), 8u);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Serializer — little-endian encoding
    // ─────────────────────────────────────────────────────────────────────────

    TEST(SerializerTest, WriteUInt8LittleEndian)
    {
        std::array<std::byte, 1> buf{};
        auto ser = makeSer(buf);
        ASSERT_EQ(ser.writeUInt8(0xABu), Status::Ok);
        EXPECT_EQ(buf[0], std::byte{0xAB});
    }

    TEST(SerializerTest, WriteUInt16LittleEndian)
    {
        std::array<std::byte, 2> buf{};
        auto ser = makeSer(buf);
        ASSERT_EQ(ser.writeUInt16(0x0102u), Status::Ok);
        // LE: low byte first
        EXPECT_EQ(buf[0], std::byte{0x02});
        EXPECT_EQ(buf[1], std::byte{0x01});
    }

    TEST(SerializerTest, WriteUInt32LittleEndian)
    {
        std::array<std::byte, 4> buf{};
        auto ser = makeSer(buf);
        ASSERT_EQ(ser.writeUInt32(0x01020304u), Status::Ok);
        EXPECT_EQ(buf[0], std::byte{0x04});
        EXPECT_EQ(buf[1], std::byte{0x03});
        EXPECT_EQ(buf[2], std::byte{0x02});
        EXPECT_EQ(buf[3], std::byte{0x01});
    }

    TEST(SerializerTest, WriteUInt64LittleEndian)
    {
        std::array<std::byte, 8> buf{};
        auto ser = makeSer(buf);
        ASSERT_EQ(ser.writeUInt64(0x0102030405060708ULL), Status::Ok);
        EXPECT_EQ(buf[0], std::byte{0x08});
        EXPECT_EQ(buf[1], std::byte{0x07});
        EXPECT_EQ(buf[2], std::byte{0x06});
        EXPECT_EQ(buf[3], std::byte{0x05});
        EXPECT_EQ(buf[4], std::byte{0x04});
        EXPECT_EQ(buf[5], std::byte{0x03});
        EXPECT_EQ(buf[6], std::byte{0x02});
        EXPECT_EQ(buf[7], std::byte{0x01});
    }

    TEST(SerializerTest, WriteInt32NegativeValue)
    {
        std::array<std::byte, 4> buf{};
        auto ser = makeSer(buf);
        ASSERT_EQ(ser.writeInt32(-1), Status::Ok);
        // -1 in two's complement is 0xFFFFFFFF, LE = FF FF FF FF
        EXPECT_EQ(buf[0], std::byte{0xFF});
        EXPECT_EQ(buf[1], std::byte{0xFF});
        EXPECT_EQ(buf[2], std::byte{0xFF});
        EXPECT_EQ(buf[3], std::byte{0xFF});
    }

    TEST(SerializerTest, WriteBoolFalseIsZeroByte)
    {
        std::array<std::byte, 1> buf{};
        auto ser = makeSer(buf);
        ASSERT_EQ(ser.writeBool(false), Status::Ok);
        EXPECT_EQ(buf[0], std::byte{0x00});
    }

    TEST(SerializerTest, WriteBoolTrueIsOneByte)
    {
        std::array<std::byte, 1> buf{};
        auto ser = makeSer(buf);
        ASSERT_EQ(ser.writeBool(true), Status::Ok);
        EXPECT_EQ(buf[0], std::byte{0x01});
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Serializer — cursor advancement
    // ─────────────────────────────────────────────────────────────────────────

    TEST(SerializerTest, CursorAdvancesAfterEachWrite)
    {
        std::array<std::byte, 16> buf{};
        auto ser = makeSer(buf);

        ASSERT_EQ(ser.writeUInt8(1u), Status::Ok);
        EXPECT_EQ(ser.position(), 1u);

        ASSERT_EQ(ser.writeUInt16(2u), Status::Ok);
        EXPECT_EQ(ser.position(), 3u);

        ASSERT_EQ(ser.writeUInt32(3u), Status::Ok);
        EXPECT_EQ(ser.position(), 7u);

        ASSERT_EQ(ser.writeUInt64(4u), Status::Ok);
        EXPECT_EQ(ser.position(), 15u);

        ASSERT_EQ(ser.writeBool(false), Status::Ok);
        EXPECT_EQ(ser.position(), 16u);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Serializer — overflow detection
    // ─────────────────────────────────────────────────────────────────────────

    TEST(SerializerTest, WriteUInt8OverflowReturnsIoError)
    {
        std::array<std::byte, 0> buf{};
        auto ser = makeSer(buf);
        EXPECT_EQ(ser.writeUInt8(0), Status::IoError);
    }

    TEST(SerializerTest, WriteUInt32OverflowReturnsIoError)
    {
        std::array<std::byte, 3> buf{};
        auto ser = makeSer(buf);
        EXPECT_EQ(ser.writeUInt32(0), Status::IoError);
    }

    TEST(SerializerTest, OverflowDoesNotAdvanceCursor)
    {
        std::array<std::byte, 2> buf{};
        auto ser = makeSer(buf);
        ASSERT_EQ(ser.writeUInt16(0x1234u), Status::Ok);
        EXPECT_EQ(ser.position(), 2u);
        EXPECT_EQ(ser.writeUInt8(0), Status::IoError);
        EXPECT_EQ(ser.position(), 2u); // cursor unchanged
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Serializer — writeBytes
    // ─────────────────────────────────────────────────────────────────────────

    TEST(SerializerTest, WriteBytesAppendsRawData)
    {
        std::array<std::byte, 4> buf{};
        auto ser = makeSer(buf);

        const std::array<std::byte, 4> src = {std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE},
                                              std::byte{0xEF}};
        ASSERT_EQ(ser.writeBytes(src), Status::Ok);
        EXPECT_EQ(buf, src);
    }

    TEST(SerializerTest, WriteBytesOverflowReturnsIoError)
    {
        std::array<std::byte, 2> buf{};
        auto ser = makeSer(buf);
        std::array<std::byte, 3> src{};
        EXPECT_EQ(ser.writeBytes(src), Status::IoError);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Serializer — writeString
    // ─────────────────────────────────────────────────────────────────────────

    TEST(SerializerTest, WriteStringEncodesFourByteLengthPlusBytesThenContent)
    {
        std::array<std::byte, 8> buf{};
        auto ser = makeSer(buf);
        ASSERT_EQ(ser.writeString("abc"), Status::Ok);
        // Length prefix LE: 0x03 00 00 00
        EXPECT_EQ(buf[0], std::byte{0x03});
        EXPECT_EQ(buf[1], std::byte{0x00});
        EXPECT_EQ(buf[2], std::byte{0x00});
        EXPECT_EQ(buf[3], std::byte{0x00});
        // Content: 'a' 'b' 'c'
        EXPECT_EQ(buf[4], std::byte{'a'});
        EXPECT_EQ(buf[5], std::byte{'b'});
        EXPECT_EQ(buf[6], std::byte{'c'});
        EXPECT_EQ(ser.position(), 7u);
    }

    TEST(SerializerTest, WriteEmptyStringWritesOnlyLengthPrefix)
    {
        std::array<std::byte, 4> buf{};
        auto ser = makeSer(buf);
        ASSERT_EQ(ser.writeString(""), Status::Ok);
        EXPECT_EQ(ser.position(), 4u);
        for (auto b : buf)
        {
            EXPECT_EQ(b, std::byte{0});
        }
    }

    TEST(SerializerTest, WriteStringOverflowReturnsIoError)
    {
        std::array<std::byte, 4> buf{}; // just enough for length, not content
        auto ser = makeSer(buf);
        EXPECT_EQ(ser.writeString("hello"), Status::IoError);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Serializer — writeUUID
    // ─────────────────────────────────────────────────────────────────────────

    TEST(SerializerTest, WriteUUIDWritesSixteenBytes)
    {
        std::array<std::byte, 16> buf{};
        auto ser = makeSer(buf);

        std::array<std::uint8_t, 16> uuid{};
        for (std::uint8_t i = 0; i < 16; ++i)
        {
            uuid[i] = i;
        }

        ASSERT_EQ(ser.writeUUID(uuid), Status::Ok);
        EXPECT_EQ(ser.position(), 16u);

        for (std::size_t i = 0; i < 16; ++i)
        {
            EXPECT_EQ(static_cast<std::uint8_t>(buf[i]), static_cast<std::uint8_t>(i));
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Deserializer — basic construction
    // ─────────────────────────────────────────────────────────────────────────

    TEST(DeserializerTest, InitialPositionIsZero)
    {
        std::array<std::byte, 8> buf{};
        auto des = makeDes(buf);
        EXPECT_EQ(des.position(), 0u);
        EXPECT_EQ(des.remaining(), 8u);
    }

    TEST(DeserializerTest, ResetRestoresCursor)
    {
        std::array<std::byte, 4> buf{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}};
        auto des = makeDes(buf);
        std::uint32_t v = 0;
        ASSERT_EQ(des.readUInt32(v), Status::Ok);
        des.reset();
        EXPECT_EQ(des.position(), 0u);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Deserializer — little-endian decoding
    // ─────────────────────────────────────────────────────────────────────────

    TEST(DeserializerTest, ReadUInt8)
    {
        std::array<std::byte, 1> buf{std::byte{0xAB}};
        auto des = makeDes(buf);
        std::uint8_t v = 0;
        ASSERT_EQ(des.readUInt8(v), Status::Ok);
        EXPECT_EQ(v, 0xABu);
    }

    TEST(DeserializerTest, ReadUInt16LittleEndian)
    {
        std::array<std::byte, 2> buf{std::byte{0x02}, std::byte{0x01}};
        auto des = makeDes(buf);
        std::uint16_t v = 0;
        ASSERT_EQ(des.readUInt16(v), Status::Ok);
        EXPECT_EQ(v, 0x0102u);
    }

    TEST(DeserializerTest, ReadUInt32LittleEndian)
    {
        std::array<std::byte, 4> buf{std::byte{0x04}, std::byte{0x03}, std::byte{0x02},
                                     std::byte{0x01}};
        auto des = makeDes(buf);
        std::uint32_t v = 0;
        ASSERT_EQ(des.readUInt32(v), Status::Ok);
        EXPECT_EQ(v, 0x01020304u);
    }

    TEST(DeserializerTest, ReadUInt64LittleEndian)
    {
        std::array<std::byte, 8> buf{std::byte{0x08}, std::byte{0x07}, std::byte{0x06},
                                     std::byte{0x05}, std::byte{0x04}, std::byte{0x03},
                                     std::byte{0x02}, std::byte{0x01}};
        auto des = makeDes(buf);
        std::uint64_t v = 0;
        ASSERT_EQ(des.readUInt64(v), Status::Ok);
        EXPECT_EQ(v, 0x0102030405060708ULL);
    }

    TEST(DeserializerTest, ReadInt32Negative)
    {
        std::array<std::byte, 4> buf{std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF},
                                     std::byte{0xFF}};
        auto des = makeDes(buf);
        std::int32_t v = 0;
        ASSERT_EQ(des.readInt32(v), Status::Ok);
        EXPECT_EQ(v, -1);
    }

    TEST(DeserializerTest, ReadBoolFalse)
    {
        std::array<std::byte, 1> buf{std::byte{0x00}};
        auto des = makeDes(buf);
        bool v = true;
        ASSERT_EQ(des.readBool(v), Status::Ok);
        EXPECT_FALSE(v);
    }

    TEST(DeserializerTest, ReadBoolNonZeroIsTrue)
    {
        std::array<std::byte, 1> buf{std::byte{0x42}};
        auto des = makeDes(buf);
        bool v = false;
        ASSERT_EQ(des.readBool(v), Status::Ok);
        EXPECT_TRUE(v);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Deserializer — cursor advancement
    // ─────────────────────────────────────────────────────────────────────────

    TEST(DeserializerTest, CursorAdvancesAfterEachRead)
    {
        std::array<std::byte, 16> buf{};
        auto des = makeDes(buf);
        std::uint8_t u8 = 0;
        std::uint16_t u16 = 0;
        std::uint32_t u32 = 0;
        std::uint64_t u64 = 0;
        bool b = false;

        ASSERT_EQ(des.readUInt8(u8), Status::Ok);
        EXPECT_EQ(des.position(), 1u);
        ASSERT_EQ(des.readUInt16(u16), Status::Ok);
        EXPECT_EQ(des.position(), 3u);
        ASSERT_EQ(des.readUInt32(u32), Status::Ok);
        EXPECT_EQ(des.position(), 7u);
        ASSERT_EQ(des.readUInt64(u64), Status::Ok);
        EXPECT_EQ(des.position(), 15u);
        ASSERT_EQ(des.readBool(b), Status::Ok);
        EXPECT_EQ(des.position(), 16u);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Deserializer — underflow detection
    // ─────────────────────────────────────────────────────────────────────────

    TEST(DeserializerTest, ReadUInt8UnderflowReturnsIoError)
    {
        std::array<std::byte, 0> buf{};
        auto des = makeDes(buf);
        std::uint8_t v = 0;
        EXPECT_EQ(des.readUInt8(v), Status::IoError);
    }

    TEST(DeserializerTest, ReadUInt32UnderflowReturnsIoError)
    {
        std::array<std::byte, 3> buf{};
        auto des = makeDes(buf);
        std::uint32_t v = 0;
        EXPECT_EQ(des.readUInt32(v), Status::IoError);
    }

    TEST(DeserializerTest, UnderflowDoesNotAdvanceCursor)
    {
        std::array<std::byte, 2> buf{std::byte{0x01}, std::byte{0x02}};
        auto des = makeDes(buf);
        std::uint16_t v16 = 0;
        ASSERT_EQ(des.readUInt16(v16), Status::Ok);
        EXPECT_EQ(des.position(), 2u);
        std::uint8_t v8 = 0;
        EXPECT_EQ(des.readUInt8(v8), Status::IoError);
        EXPECT_EQ(des.position(), 2u); // cursor unchanged
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Deserializer — readBytes
    // ─────────────────────────────────────────────────────────────────────────

    TEST(DeserializerTest, ReadBytesReturnsSubspan)
    {
        std::array<std::byte, 4> buf{std::byte{0x01}, std::byte{0x02}, std::byte{0x03},
                                     std::byte{0x04}};
        auto des = makeDes(buf);
        std::span<const std::byte> out;
        ASSERT_EQ(des.readBytes(4, out), Status::Ok);
        EXPECT_EQ(out.size(), 4u);
        EXPECT_EQ(out[0], std::byte{0x01});
        EXPECT_EQ(out[3], std::byte{0x04});
    }

    TEST(DeserializerTest, ReadBytesUnderflowReturnsIoError)
    {
        std::array<std::byte, 2> buf{};
        auto des = makeDes(buf);
        std::span<const std::byte> out;
        EXPECT_EQ(des.readBytes(3, out), Status::IoError);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Deserializer — readUUID
    // ─────────────────────────────────────────────────────────────────────────

    TEST(DeserializerTest, ReadUUIDReadsSixteenBytes)
    {
        std::array<std::byte, 16> buf{};
        for (std::size_t i = 0; i < 16; ++i)
        {
            buf[i] = static_cast<std::byte>(i);
        }
        auto des = makeDes(buf);
        std::array<std::uint8_t, 16> uuid{};
        ASSERT_EQ(des.readUUID(uuid), Status::Ok);
        EXPECT_EQ(des.position(), 16u);
        for (std::size_t i = 0; i < 16; ++i)
        {
            EXPECT_EQ(uuid[i], static_cast<std::uint8_t>(i));
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Deserializer — readString
    // ─────────────────────────────────────────────────────────────────────────

    TEST(DeserializerTest, ReadStringDecodesCorrectly)
    {
        std::array<std::byte, 8> buf{};
        // Manually encode "abc" with 4-byte LE length
        buf[0] = std::byte{0x03};
        buf[4] = std::byte{'a'};
        buf[5] = std::byte{'b'};
        buf[6] = std::byte{'c'};

        auto des = makeDes(buf);
        std::string out;
        ASSERT_EQ(des.readString(out), Status::Ok);
        EXPECT_EQ(out, "abc");
        EXPECT_EQ(des.position(), 7u);
    }

    TEST(DeserializerTest, ReadEmptyString)
    {
        std::array<std::byte, 4> buf{};
        auto des = makeDes(buf);
        std::string out = "existing";
        ASSERT_EQ(des.readString(out), Status::Ok);
        EXPECT_EQ(out, "");
    }

    TEST(DeserializerTest, ReadStringUnderflowReturnsIoError)
    {
        // Length prefix says 100 bytes but buffer is only 4 bytes total
        std::array<std::byte, 4> buf{};
        buf[0] = std::byte{100};
        auto des = makeDes(buf);
        std::string out = "unchanged";
        EXPECT_EQ(des.readString(out), Status::IoError);
        // out must be unchanged
        EXPECT_EQ(out, "unchanged");
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Round-trip: Serializer → Deserializer
    // ─────────────────────────────────────────────────────────────────────────

    TEST(RoundTripTest, Uint8RoundTrip)
    {
        std::array<std::byte, 1> buf{};
        ASSERT_EQ(makeSer(buf).writeUInt8(0xFEu), Status::Ok);
        std::uint8_t v = 0;
        ASSERT_EQ(makeDes(buf).readUInt8(v), Status::Ok);
        EXPECT_EQ(v, 0xFEu);
    }

    TEST(RoundTripTest, Uint16RoundTrip)
    {
        std::array<std::byte, 2> buf{};
        ASSERT_EQ(makeSer(buf).writeUInt16(0xCAFEu), Status::Ok);
        std::uint16_t v = 0;
        ASSERT_EQ(makeDes(buf).readUInt16(v), Status::Ok);
        EXPECT_EQ(v, 0xCAFEu);
    }

    TEST(RoundTripTest, Uint32RoundTrip)
    {
        std::array<std::byte, 4> buf{};
        ASSERT_EQ(makeSer(buf).writeUInt32(0xDEADBEEFu), Status::Ok);
        std::uint32_t v = 0;
        ASSERT_EQ(makeDes(buf).readUInt32(v), Status::Ok);
        EXPECT_EQ(v, 0xDEADBEEFu);
    }

    TEST(RoundTripTest, Uint64RoundTrip)
    {
        std::array<std::byte, 8> buf{};
        ASSERT_EQ(makeSer(buf).writeUInt64(0xCAFEBABEDEADBEEFULL), Status::Ok);
        std::uint64_t v = 0;
        ASSERT_EQ(makeDes(buf).readUInt64(v), Status::Ok);
        EXPECT_EQ(v, 0xCAFEBABEDEADBEEFULL);
    }

    TEST(RoundTripTest, Int32NegativeRoundTrip)
    {
        std::array<std::byte, 4> buf{};
        ASSERT_EQ(makeSer(buf).writeInt32(-42), Status::Ok);
        std::int32_t v = 0;
        ASSERT_EQ(makeDes(buf).readInt32(v), Status::Ok);
        EXPECT_EQ(v, -42);
    }

    TEST(RoundTripTest, BoolRoundTrip)
    {
        std::array<std::byte, 2> buf{};
        auto ser = makeSer(buf);
        ASSERT_EQ(ser.writeBool(true), Status::Ok);
        ASSERT_EQ(ser.writeBool(false), Status::Ok);

        auto des = makeDes(buf);
        bool a = false, b = true;
        ASSERT_EQ(des.readBool(a), Status::Ok);
        ASSERT_EQ(des.readBool(b), Status::Ok);
        EXPECT_TRUE(a);
        EXPECT_FALSE(b);
    }

    TEST(RoundTripTest, StringRoundTrip)
    {
        std::array<std::byte, 64> buf{};
        auto ser = makeSer(buf);
        ASSERT_EQ(ser.writeString("hello, HamDB!"), Status::Ok);

        auto des = makeDes(buf);
        std::string out;
        ASSERT_EQ(des.readString(out), Status::Ok);
        EXPECT_EQ(out, "hello, HamDB!");
    }

    TEST(RoundTripTest, UUIDRoundTrip)
    {
        std::array<std::byte, 16> buf{};
        std::array<std::uint8_t, 16> uuid{};
        for (std::uint8_t i = 0; i < 16; ++i)
        {
            uuid[i] = static_cast<std::uint8_t>(0xA0u + i);
        }

        ASSERT_EQ(makeSer(buf).writeUUID(uuid), Status::Ok);
        std::array<std::uint8_t, 16> got{};
        ASSERT_EQ(makeDes(buf).readUUID(got), Status::Ok);
        EXPECT_EQ(uuid, got);
    }

    TEST(RoundTripTest, MixedTypesRoundTrip)
    {
        std::array<std::byte, 128> buf{};
        auto ser = makeSer(buf);
        ASSERT_EQ(ser.writeUInt8(7u), Status::Ok);
        ASSERT_EQ(ser.writeUInt32(0xDEADBEEFu), Status::Ok);
        ASSERT_EQ(ser.writeBool(true), Status::Ok);
        ASSERT_EQ(ser.writeString("hamdb"), Status::Ok);
        ASSERT_EQ(ser.writeUInt64(0xCAFEBABEDEADBEEFULL), Status::Ok);
        ASSERT_EQ(ser.writeInt32(-1), Status::Ok);

        const std::size_t total = ser.position();

        auto des = makeDes(buf);
        std::uint8_t u8 = 0;
        std::uint32_t u32 = 0;
        bool b = false;
        std::string str;
        std::uint64_t u64 = 0;
        std::int32_t i32 = 0;

        ASSERT_EQ(des.readUInt8(u8), Status::Ok);
        EXPECT_EQ(u8, 7u);
        ASSERT_EQ(des.readUInt32(u32), Status::Ok);
        EXPECT_EQ(u32, 0xDEADBEEFu);
        ASSERT_EQ(des.readBool(b), Status::Ok);
        EXPECT_TRUE(b);
        ASSERT_EQ(des.readString(str), Status::Ok);
        EXPECT_EQ(str, "hamdb");
        ASSERT_EQ(des.readUInt64(u64), Status::Ok);
        EXPECT_EQ(u64, 0xCAFEBABEDEADBEEFULL);
        ASSERT_EQ(des.readInt32(i32), Status::Ok);
        EXPECT_EQ(i32, -1);
        EXPECT_EQ(des.position(), total);
    }

    TEST(RoundTripTest, RemainingBytesAfterPartialWrite)
    {
        std::array<std::byte, 10> buf{};
        auto ser = makeSer(buf);
        ASSERT_EQ(ser.writeUInt32(42u), Status::Ok);
        EXPECT_EQ(ser.remaining(), 6u);
        EXPECT_EQ(ser.position(), 4u);
    }

} // namespace hamdb
