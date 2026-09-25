#include "storage/rid.hpp"
#include "storage/tuple.hpp"
#include "utils/deserializer.hpp"
#include "utils/serializer.hpp"
#include "wal/log_manager.hpp"
#include "wal/log_record.hpp"
#include <gtest/gtest.h>
#include <vector>

namespace hamdb
{

    TEST(WalTest, LogManagerAppendAndFlush)
    {
        LogManager log_manager;

        EXPECT_EQ(log_manager.getNextLSN(), 1);
        EXPECT_EQ(log_manager.getPersistentLSN(), kInvalidLSN);

        LogRecord begin_record(100, LogRecordType::BEGIN, kInvalidLSN);
        lsn_t lsn1 = log_manager.append(begin_record);
        EXPECT_EQ(lsn1, 1);
        EXPECT_EQ(begin_record.getLSN(), 1);
        EXPECT_EQ(log_manager.getNextLSN(), 2);

        LogRecord commit_record(100, LogRecordType::COMMIT, 1);
        lsn_t lsn2 = log_manager.append(commit_record);
        EXPECT_EQ(lsn2, 2);
        EXPECT_EQ(commit_record.getLSN(), 2);

        log_manager.flush(1);
        EXPECT_EQ(log_manager.getPersistentLSN(), 1);

        log_manager.flushAll();
        EXPECT_EQ(log_manager.getPersistentLSN(), 2);
    }

    TEST(WalTest, LogRecordSerialization)
    {
        std::vector<std::byte> payload = {std::byte{0xDE}, std::byte{0xAD}};
        Tuple tuple(payload);
        RID rid(42, 7);
        LogRecord record(100, LogRecordType::INSERT, kInvalidLSN, rid, tuple);
        record.setLSN(1);

        std::vector<std::byte> buffer(record.getSerializedSize());
        Serializer serializer(buffer);
        EXPECT_EQ(record.serialize(serializer), Status::Ok);

        LogRecord decoded;
        Deserializer deserializer(buffer);
        EXPECT_EQ(decoded.deserialize(deserializer), Status::Ok);

        EXPECT_EQ(decoded.getTxnId(), 100);
        EXPECT_EQ(decoded.getRecordType(), LogRecordType::INSERT);
        EXPECT_EQ(decoded.getPrevLSN(), kInvalidLSN);
        EXPECT_EQ(decoded.getLSN(), 1);
        EXPECT_EQ(decoded.getRID().getPageId(), 42);
        EXPECT_EQ(decoded.getRID().getSlotId(), 7);
        EXPECT_EQ(decoded.getTuple().size(), 2);
        EXPECT_EQ(decoded.getTuple().data()[0], std::byte{0xDE});
        EXPECT_EQ(decoded.getTuple().data()[1], std::byte{0xAD});
    }

    TEST(WalTest, LogRecordUpdateSerialization)
    {
        std::vector<std::byte> old_payload = {std::byte{0xAA}};
        std::vector<std::byte> new_payload = {std::byte{0xBB}, std::byte{0xCC}};
        Tuple old_tuple(old_payload);
        Tuple new_tuple(new_payload);
        RID rid(10, 2);
        LogRecord record(200, LogRecordType::UPDATE, 5, rid, old_tuple, new_tuple);
        record.setLSN(6);

        std::vector<std::byte> buffer(record.getSerializedSize());
        Serializer serializer(buffer);
        EXPECT_EQ(record.serialize(serializer), Status::Ok);

        LogRecord decoded;
        Deserializer deserializer(buffer);
        EXPECT_EQ(decoded.deserialize(deserializer), Status::Ok);

        EXPECT_EQ(decoded.getTxnId(), 200);
        EXPECT_EQ(decoded.getRecordType(), LogRecordType::UPDATE);
        EXPECT_EQ(decoded.getPrevLSN(), 5);
        EXPECT_EQ(decoded.getLSN(), 6);
        EXPECT_EQ(decoded.getRID().getPageId(), 10);
        EXPECT_EQ(decoded.getRID().getSlotId(), 2);

        EXPECT_EQ(decoded.getOldTuple().size(), 1);
        EXPECT_EQ(decoded.getOldTuple().data()[0], std::byte{0xAA});

        EXPECT_EQ(decoded.getNewTuple().size(), 2);
        EXPECT_EQ(decoded.getNewTuple().data()[0], std::byte{0xBB});
        EXPECT_EQ(decoded.getNewTuple().data()[1], std::byte{0xCC});
    }

} // namespace hamdb
