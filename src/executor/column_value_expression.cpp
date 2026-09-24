#include "executor/column_value_expression.hpp"
#include "utils/deserializer.hpp"
#include <stdexcept>

namespace hamdb {

Value ColumnValueExpression::evaluate(const Tuple& tuple, const Schema& schema) const {
    if (col_idx_ >= schema.getColumnCount()) {
        throw std::runtime_error("Column index out of bounds");
    }

    Deserializer deserializer(tuple.data());
    
    for (uint32_t i = 0; i <= col_idx_; ++i) {
        const auto& col = schema.getColumn(i);
        
        if (i == col_idx_) {
            if (col.getType() == ColumnType::Integer) {
                int32_t val = 0;
                if (deserializer.readInt32(val) != Status::Ok) throw std::runtime_error("Deserialization failed");
                return Value(val);
            } else if (col.getType() == ColumnType::Boolean) {
                bool val = false;
                if (deserializer.readBool(val) != Status::Ok) throw std::runtime_error("Deserialization failed");
                return Value(val);
            } else if (col.getType() == ColumnType::Varchar) {
                std::string val;
                if (deserializer.readString(val) != Status::Ok) throw std::runtime_error("Deserialization failed");
                return Value(std::move(val));
            } else {
                throw std::runtime_error("Unsupported column type");
            }
        } else {
            if (col.getType() == ColumnType::Integer) {
                int32_t val = 0;
                if (deserializer.readInt32(val) != Status::Ok) throw std::runtime_error("Deserialization failed");
            } else if (col.getType() == ColumnType::Boolean) {
                bool val = false;
                if (deserializer.readBool(val) != Status::Ok) throw std::runtime_error("Deserialization failed");
            } else if (col.getType() == ColumnType::Varchar) {
                std::string val;
                if (deserializer.readString(val) != Status::Ok) throw std::runtime_error("Deserialization failed");
            } else if (col.getType() == ColumnType::Float) {
                throw std::runtime_error("Float not supported in deserialization skip");
            }
        }
    }
    
    throw std::runtime_error("Should not reach here");
}

} // namespace hamdb
