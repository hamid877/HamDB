#include "catalog/catalog_manager.hpp"
#include "storage/database_metadata.hpp"

namespace hamdb {

CatalogManager::CatalogManager(BufferPoolManager* bpm) : bpm_(bpm) {
    auto status = loadFromDisk();
    if (status != Status::Ok) {
        // Log or handle error, but for this milestone we can assume
        // a new database might not have valid catalog data yet.
    }
}

Status CatalogManager::createTable(const std::string& table_name, const Schema& schema, TableInfo*& out_info) {
    if (tables_.find(table_name) != tables_.end()) {
        return Status::AlreadyExists;
    }
    
    PageId heap_root_page = kInvalidPageId;
    WritePageGuard heap_guard;
    if (auto status = bpm_->newPageGuard(heap_root_page, heap_guard); status != Status::Ok) {
        return status;
    }
    
    PageId index_root_page = kInvalidPageId;
    WritePageGuard index_guard;
    if (auto status = bpm_->newPageGuard(index_root_page, index_guard); status != Status::Ok) {
        return status;
    }

    auto table_info = std::make_unique<TableInfo>(next_table_id_++, table_name, heap_root_page, index_root_page, schema);
    out_info = table_info.get();
    tables_[table_name] = std::move(table_info);
    
    return saveToDisk();
}

Status CatalogManager::getTable(const std::string& table_name, TableInfo*& out_info) {
    auto it = tables_.find(table_name);
    if (it == tables_.end()) {
        return Status::NotFound;
    }
    out_info = it->second.get();
    return Status::Ok;
}

Status CatalogManager::dropTable(const std::string& table_name) {
    auto it = tables_.find(table_name);
    if (it == tables_.end()) {
        return Status::NotFound;
    }
    
    tables_.erase(it);
    
    return saveToDisk();
}

std::vector<std::string> CatalogManager::listTables() const {
    std::vector<std::string> names;
    names.reserve(tables_.size());
    for (const auto& [name, info] : tables_) {
        names.push_back(name);
    }
    return names;
}

Status CatalogManager::saveToDisk() {
    WritePageGuard guard;
    if (auto status = bpm_->fetchPageWrite(0, guard); status != Status::Ok) {
        return status;
    }
    
    auto span = guard.pageMut().data();
    auto catalog_span = span.subspan(DatabaseMetadata::kSize);
    
    Serializer serializer(catalog_span);
    
    if (auto status = serializer.writeUInt32(next_table_id_); status != Status::Ok) return status;
    if (auto status = serializer.writeUInt32(static_cast<std::uint32_t>(tables_.size())); status != Status::Ok) return status;
    
    for (const auto& [name, info] : tables_) {
        if (auto status = info->serialize(serializer); status != Status::Ok) return status;
    }
    
    guard.markDirty();
    return Status::Ok;
}

Status CatalogManager::loadFromDisk() {
    ReadPageGuard guard;
    if (auto status = bpm_->fetchPageRead(0, guard); status != Status::Ok) {
        return status;
    }
    
    auto span = guard.page().data();
    auto catalog_span = span.subspan(DatabaseMetadata::kSize);
    
    Deserializer deserializer(catalog_span);
    
    std::uint32_t next_id = 0;
    if (auto status = deserializer.readUInt32(next_id); status != Status::Ok) {
        return Status::Ok;
    }
    next_table_id_ = next_id;
    
    std::uint32_t table_count = 0;
    if (auto status = deserializer.readUInt32(table_count); status != Status::Ok) {
        return Status::Ok;
    }
    
    for (std::uint32_t i = 0; i < table_count; ++i) {
        auto info = std::make_unique<TableInfo>();
        if (auto status = info->deserialize(deserializer); status != Status::Ok) {
            return status;
        }
        tables_[info->getTableName()] = std::move(info);
    }
    
    return Status::Ok;
}

} // namespace hamdb
