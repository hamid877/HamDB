#pragma once

#include "catalog/table_info.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

namespace hamdb {

class CatalogManager {
public:
    explicit CatalogManager(BufferPoolManager* bpm);

    [[nodiscard]] Status createTable(const std::string& table_name, const Schema& schema, TableInfo*& out_info);
    
    [[nodiscard]] Status getTable(const std::string& table_name, TableInfo*& out_info);
    
    [[nodiscard]] Status dropTable(const std::string& table_name);
    
    [[nodiscard]] std::vector<std::string> listTables() const;

private:
    BufferPoolManager* bpm_;
    std::unordered_map<std::string, std::unique_ptr<TableInfo>> tables_;
    std::uint32_t next_table_id_{0};

    [[nodiscard]] Status loadFromDisk();
    [[nodiscard]] Status saveToDisk();
};

} // namespace hamdb
