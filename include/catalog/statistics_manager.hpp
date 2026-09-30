#pragma once

#include "catalog/catalog_manager.hpp"
#include "catalog/statistics.hpp"
#include "storage/disk_manager.hpp"
#include <string>
#include <unordered_map>
#include <optional>

namespace hamdb
{

    class StatisticsManager
    {
    public:
        StatisticsManager(CatalogManager* catalog, DiskManager* disk_manager);

        [[nodiscard]] Status refreshTableStatistics(const std::string& table_name);

        [[nodiscard]] std::optional<TableStatistics> getTableStatistics(const std::string& table_name) const;

    private:
        CatalogManager* catalog_;
        DiskManager* disk_manager_;
        std::unordered_map<std::string, TableStatistics> table_stats_;
    };

} // namespace hamdb
