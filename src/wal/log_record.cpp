#include "wal/log_record.hpp"

namespace hamdb {

static constexpr uint32_t HEADER_SIZE = sizeof(uint32_t) + sizeof(uint8_t) + sizeof(txn_id_t) + sizeof(lsn_t) + sizeof(lsn_t);

LogRecord::LogRecord(txn_id_t txn_id, LogRecordType record_type, lsn_t prev_lsn)
    : size_(HEADER_SIZE), record_type_(record_type), txn_id_(txn_id), prev_lsn_(prev_lsn) {}

LogRecord::LogRecord(txn_id_t txn_id, LogRecordType record_type, lsn_t prev_lsn, RID rid, const Tuple& tuple)
    : record_type_(record_type), txn_id_(txn_id), prev_lsn_(prev_lsn), rid_(rid), tuple_(tuple) {
    size_ = HEADER_SIZE + sizeof(PageId) + sizeof(uint16_t) + sizeof(uint32_t) + tuple_.size();
}

LogRecord::LogRecord(txn_id_t txn_id, LogRecordType record_type, lsn_t prev_lsn, RID rid, const Tuple& old_tuple, const Tuple& new_tuple)
    : record_type_(record_type), txn_id_(txn_id), prev_lsn_(prev_lsn), rid_(rid), old_tuple_(old_tuple), new_tuple_(new_tuple) {
    size_ = HEADER_SIZE + sizeof(PageId) + sizeof(uint16_t) + sizeof(uint32_t) + old_tuple_.size() + sizeof(uint32_t) + new_tuple_.size();
}

Status LogRecord::serialize(Serializer& serializer) const noexcept {
    if (auto status = serializer.writeUInt32(size_); status != Status::Ok) return status;
    if (auto status = serializer.writeUInt8(static_cast<uint8_t>(record_type_)); status != Status::Ok) return status;
    if (auto status = serializer.writeUInt64(txn_id_); status != Status::Ok) return status;
    if (auto status = serializer.writeUInt64(prev_lsn_); status != Status::Ok) return status;
    if (auto status = serializer.writeUInt64(lsn_); status != Status::Ok) return status;

    if (record_type_ == LogRecordType::INSERT || record_type_ == LogRecordType::DELETE) {
        if (auto status = serializer.writeUInt32(rid_.getPageId()); status != Status::Ok) return status;
        if (auto status = serializer.writeUInt16(rid_.getSlotId()); status != Status::Ok) return status;
        if (auto status = serializer.writeUInt32(static_cast<uint32_t>(tuple_.size())); status != Status::Ok) return status;
        if (auto status = serializer.writeBytes(tuple_.data()); status != Status::Ok) return status;
    } else if (record_type_ == LogRecordType::UPDATE) {
        if (auto status = serializer.writeUInt32(rid_.getPageId()); status != Status::Ok) return status;
        if (auto status = serializer.writeUInt16(rid_.getSlotId()); status != Status::Ok) return status;
        if (auto status = serializer.writeUInt32(static_cast<uint32_t>(old_tuple_.size())); status != Status::Ok) return status;
        if (auto status = serializer.writeBytes(old_tuple_.data()); status != Status::Ok) return status;
        if (auto status = serializer.writeUInt32(static_cast<uint32_t>(new_tuple_.size())); status != Status::Ok) return status;
        if (auto status = serializer.writeBytes(new_tuple_.data()); status != Status::Ok) return status;
    }

    return Status::Ok;
}

Status LogRecord::deserialize(Deserializer& deserializer) noexcept {
    if (auto status = deserializer.readUInt32(size_); status != Status::Ok) return status;
    uint8_t type;
    if (auto status = deserializer.readUInt8(type); status != Status::Ok) return status;
    record_type_ = static_cast<LogRecordType>(type);
    if (auto status = deserializer.readUInt64(txn_id_); status != Status::Ok) return status;
    if (auto status = deserializer.readUInt64(prev_lsn_); status != Status::Ok) return status;
    if (auto status = deserializer.readUInt64(lsn_); status != Status::Ok) return status;

    if (record_type_ == LogRecordType::INSERT || record_type_ == LogRecordType::DELETE) {
        PageId page_id;
        uint16_t slot_id;
        if (auto status = deserializer.readUInt32(page_id); status != Status::Ok) return status;
        if (auto status = deserializer.readUInt16(slot_id); status != Status::Ok) return status;
        rid_ = RID(page_id, slot_id);

        uint32_t tuple_size;
        if (auto status = deserializer.readUInt32(tuple_size); status != Status::Ok) return status;
        std::span<const std::byte> tuple_data;
        if (auto status = deserializer.readBytes(tuple_size, tuple_data); status != Status::Ok) return status;
        tuple_ = Tuple(tuple_data);
    } else if (record_type_ == LogRecordType::UPDATE) {
        PageId page_id;
        uint16_t slot_id;
        if (auto status = deserializer.readUInt32(page_id); status != Status::Ok) return status;
        if (auto status = deserializer.readUInt16(slot_id); status != Status::Ok) return status;
        rid_ = RID(page_id, slot_id);

        uint32_t old_tuple_size;
        if (auto status = deserializer.readUInt32(old_tuple_size); status != Status::Ok) return status;
        std::span<const std::byte> old_tuple_data;
        if (auto status = deserializer.readBytes(old_tuple_size, old_tuple_data); status != Status::Ok) return status;
        old_tuple_ = Tuple(old_tuple_data);

        uint32_t new_tuple_size;
        if (auto status = deserializer.readUInt32(new_tuple_size); status != Status::Ok) return status;
        std::span<const std::byte> new_tuple_data;
        if (auto status = deserializer.readBytes(new_tuple_size, new_tuple_data); status != Status::Ok) return status;
        new_tuple_ = Tuple(new_tuple_data);
    }

    return Status::Ok;
}

std::string_view LogRecord::recordTypeToString(LogRecordType type) noexcept {
    switch (type) {
        case LogRecordType::BEGIN: return "BEGIN";
        case LogRecordType::INSERT: return "INSERT";
        case LogRecordType::UPDATE: return "UPDATE";
        case LogRecordType::DELETE: return "DELETE";
        case LogRecordType::COMMIT: return "COMMIT";
        case LogRecordType::ABORT: return "ABORT";
        default: return "UNKNOWN";
    }
}

} // namespace hamdb
