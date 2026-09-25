#include "catalog/schema.hpp"

namespace hamdb
{

    Column::Column(std::string name, ColumnType type) : name_(std::move(name)), type_(type) {}

    const std::string& Column::getName() const noexcept
    {
        return name_;
    }

    ColumnType Column::getType() const noexcept
    {
        return type_;
    }

    Status Column::serialize(Serializer& serializer) const noexcept
    {
        if (auto status = serializer.writeString(name_); status != Status::Ok)
            return status;
        if (auto status = serializer.writeUInt8(static_cast<std::uint8_t>(type_));
            status != Status::Ok)
            return status;
        return Status::Ok;
    }

    Status Column::deserialize(Deserializer& deserializer)
    {
        if (auto status = deserializer.readString(name_); status != Status::Ok)
            return status;
        std::uint8_t type_val = 0;
        if (auto status = deserializer.readUInt8(type_val); status != Status::Ok)
            return status;
        type_ = static_cast<ColumnType>(type_val);
        return Status::Ok;
    }

    Schema::Schema(std::vector<Column> columns) : columns_(std::move(columns)) {}

    const std::vector<Column>& Schema::getColumns() const noexcept
    {
        return columns_;
    }

    const Column& Schema::getColumn(std::size_t index) const
    {
        return columns_.at(index);
    }

    std::size_t Schema::getColumnCount() const noexcept
    {
        return columns_.size();
    }

    Status Schema::serialize(Serializer& serializer) const noexcept
    {
        if (auto status = serializer.writeUInt32(static_cast<std::uint32_t>(columns_.size()));
            status != Status::Ok)
            return status;
        for (const auto& col : columns_)
        {
            if (auto status = col.serialize(serializer); status != Status::Ok)
                return status;
        }
        return Status::Ok;
    }

    Status Schema::deserialize(Deserializer& deserializer)
    {
        std::uint32_t count = 0;
        if (auto status = deserializer.readUInt32(count); status != Status::Ok)
            return status;
        columns_.clear();
        columns_.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            Column col;
            if (auto status = col.deserialize(deserializer); status != Status::Ok)
                return status;
            columns_.push_back(std::move(col));
        }
        return Status::Ok;
    }

} // namespace hamdb
