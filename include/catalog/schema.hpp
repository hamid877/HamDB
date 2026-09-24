#pragma once

#include "common/enums.hpp"
#include "utils/serializer.hpp"
#include "utils/deserializer.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace hamdb {

enum class ColumnType : std::uint8_t {
    Integer = 0,
    Float = 1,
    Boolean = 2,
    Varchar = 3
};

class Column {
public:
    Column() = default;
    Column(std::string name, ColumnType type);

    [[nodiscard]] const std::string& getName() const noexcept;
    [[nodiscard]] ColumnType getType() const noexcept;

    [[nodiscard]] Status serialize(Serializer& serializer) const noexcept;
    [[nodiscard]] Status deserialize(Deserializer& deserializer);

private:
    std::string name_;
    ColumnType type_;
};

class Schema {
public:
    Schema() = default;
    explicit Schema(std::vector<Column> columns);

    [[nodiscard]] const std::vector<Column>& getColumns() const noexcept;
    [[nodiscard]] const Column& getColumn(std::size_t index) const;
    [[nodiscard]] std::size_t getColumnCount() const noexcept;

    [[nodiscard]] Status serialize(Serializer& serializer) const noexcept;
    [[nodiscard]] Status deserialize(Deserializer& deserializer);

private:
    std::vector<Column> columns_;
};

} // namespace hamdb
