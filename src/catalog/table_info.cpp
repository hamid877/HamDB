#include "catalog/table_info.hpp"

namespace hamdb {

TableInfo::TableInfo(std::uint32_t table_id, std::string table_name, PageId heap_root_page, PageId index_root_page, Schema schema) // NOLINT
    : table_id_(table_id), table_name_(std::move(table_name)), heap_root_page_(heap_root_page), index_root_page_(index_root_page), schema_(std::move(schema)) {}

std::uint32_t TableInfo::getTableId() const noexcept {
    return table_id_;
}

const std::string& TableInfo::getTableName() const noexcept {
    return table_name_;
}

PageId TableInfo::getHeapRootPage() const noexcept {
    return heap_root_page_;
}

PageId TableInfo::getIndexRootPage() const noexcept {
    return index_root_page_;
}

const Schema& TableInfo::getSchema() const noexcept {
    return schema_;
}

Status TableInfo::serialize(Serializer& serializer) const noexcept {
    if (auto status = serializer.writeUInt32(table_id_); status != Status::Ok) return status;
    if (auto status = serializer.writeString(table_name_); status != Status::Ok) return status;
    if (auto status = serializer.writeUInt32(heap_root_page_); status != Status::Ok) return status;
    if (auto status = serializer.writeUInt32(index_root_page_); status != Status::Ok) return status;
    return schema_.serialize(serializer);
}

Status TableInfo::deserialize(Deserializer& deserializer) {
    if (auto status = deserializer.readUInt32(table_id_); status != Status::Ok) return status;
    if (auto status = deserializer.readString(table_name_); status != Status::Ok) return status;
    if (auto status = deserializer.readUInt32(heap_root_page_); status != Status::Ok) return status;
    if (auto status = deserializer.readUInt32(index_root_page_); status != Status::Ok) return status;
    return schema_.deserialize(deserializer);
}

} // namespace hamdb
