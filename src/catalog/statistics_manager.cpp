#include "catalog/statistics_manager.hpp"
#include "executor/column_value_expression.hpp"
#include "storage/table_heap.hpp"
#include <unordered_set>

namespace hamdb
{

    StatisticsManager::StatisticsManager(CatalogManager* catalog, DiskManager* disk_manager)
        : catalog_(catalog), disk_manager_(disk_manager)
    {
    }

    Status StatisticsManager::refreshTableStatistics(const std::string& table_name)
    {
        TableInfo* table_info = nullptr;
        if (catalog_->getTable(table_name, table_info) != Status::Ok)
        {
            return Status::NotFound;
        }

        auto heap_opt = TableHeap::open(*disk_manager_, table_info->getHeapRootPage());
        if (!heap_opt)
        {
            return Status::IoError;
        }

        const Schema& schema = table_info->getSchema();
        std::size_t col_count = schema.getColumnCount();

        TableStatistics stats;
        stats.column_stats.resize(col_count);

        std::vector<std::unordered_set<std::string>> distinct_values(col_count);

        auto it = heap_opt->begin();
        while (it != heap_opt->end())
        {
            Tuple tuple = *it;
            ++it;

            stats.row_count++;

            for (std::size_t i = 0; i < col_count; ++i)
            {
                ColumnValueExpression col_expr(i);
                Value val = col_expr.evaluate(tuple, schema);

                if (val.isNull())
                {
                    stats.column_stats[i].null_count++;
                    continue;
                }

                std::string str_val;
                if (val.getType() == TypeId::Integer)
                    str_val = std::to_string(val.getAsInteger());
                else if (val.getType() == TypeId::Boolean)
                    str_val = val.getAsBoolean() ? "t" : "f";
                else if (val.getType() == TypeId::Varchar)
                    str_val = val.getAsVarchar();
                
                distinct_values[i].insert(str_val);

                if (val.getType() == TypeId::Integer || val.getType() == TypeId::Varchar || val.getType() == TypeId::Boolean)
                {
                    if (!stats.column_stats[i].min_value || val.compareLessThan(*stats.column_stats[i].min_value).getAsBoolean())
                    {
                        stats.column_stats[i].min_value = val;
                    }
                    if (!stats.column_stats[i].max_value || val.compareGreaterThan(*stats.column_stats[i].max_value).getAsBoolean())
                    {
                        stats.column_stats[i].max_value = val;
                    }
                }
            }
        }

        for (std::size_t i = 0; i < col_count; ++i)
        {
            stats.column_stats[i].distinct_count = distinct_values[i].size();
        }

        table_stats_[table_name] = stats;
        return Status::Ok;
    }

    std::optional<TableStatistics> StatisticsManager::getTableStatistics(const std::string& table_name) const
    {
        auto it = table_stats_.find(table_name);
        if (it != table_stats_.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

} // namespace hamdb
