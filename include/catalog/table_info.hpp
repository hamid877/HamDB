#pragma once

#include "catalog/schema.hpp"
#include "common/constants.hpp"

namespace hamdb
{

    class TableInfo
    {
    public:
        TableInfo() = default;
        TableInfo(std::uint32_t table_id, std::string table_name, PageId heap_root_page,
                  PageId index_root_page, Schema schema); // NOLINT

        [[nodiscard]] std::uint32_t getTableId() const noexcept;
        [[nodiscard]] const std::string& getTableName() const noexcept;
        [[nodiscard]] PageId getHeapRootPage() const noexcept;
        [[nodiscard]] PageId getIndexRootPage() const noexcept;
        [[nodiscard]] const Schema& getSchema() const noexcept;

        [[nodiscard]] Status serialize(Serializer& serializer) const noexcept;
        [[nodiscard]] Status deserialize(Deserializer& deserializer);

    private:
        std::uint32_t table_id_{0};
        std::string table_name_;
        PageId heap_root_page_{kInvalidPageId};
        PageId index_root_page_{kInvalidPageId};
        Schema schema_;
    };

} // namespace hamdb
