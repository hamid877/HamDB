#include "server/wire_protocol.hpp"
#include <cstring>

namespace hamdb::server {

namespace {

void writeInt32(std::string& out, int32_t val) {
    out.append(reinterpret_cast<const char*>(&val), sizeof(val));
}

int32_t readInt32(const std::string& data, size_t& offset) {
    if (offset + sizeof(int32_t) > data.size()) throw std::runtime_error("Invalid payload size");
    int32_t val;
    std::memcpy(&val, data.data() + offset, sizeof(val));
    offset += sizeof(int32_t);
    return val;
}

void writeString(std::string& out, const std::string& val) {
    writeInt32(out, static_cast<int32_t>(val.size()));
    out.append(val);
}

std::string readString(const std::string& data, size_t& offset) {
    int32_t len = readInt32(data, offset);
    if (len < 0 || offset + len > data.size()) throw std::runtime_error("Invalid string length");
    std::string val = data.substr(offset, len);
    offset += len;
    return val;
}

} // namespace

std::string WireProtocol::serializeRequest(const RequestMessage& req) {
    std::string out;
    out.push_back(static_cast<char>(req.version));
    out.push_back(static_cast<char>(req.type));
    writeInt32(out, req.request_id);
    writeString(out, req.query);
    return out;
}

RequestMessage WireProtocol::deserializeRequest(const std::string& data) {
    if (data.size() < 2) throw std::runtime_error("Invalid request payload size");
    RequestMessage req;
    req.version = static_cast<uint8_t>(data[0]);
    req.type = static_cast<MessageType>(static_cast<uint8_t>(data[1]));
    size_t offset = 2;
    req.request_id = readInt32(data, offset);
    req.query = readString(data, offset);
    return req;
}

std::string WireProtocol::serializeResponse(const ResponseMessage& res) {
    std::string out;
    out.push_back(static_cast<char>(res.version));
    out.push_back(static_cast<char>(res.type));
    writeInt32(out, res.request_id);
    out.push_back(res.success ? 1 : 0);
    if (!res.success) {
        writeString(out, res.error_message);
    } else {
        writeInt32(out, static_cast<int32_t>(res.columns.size()));
        for (const auto& col : res.columns) {
            writeString(out, col.name);
            out.push_back(static_cast<char>(col.type));
        }
        writeInt32(out, static_cast<int32_t>(res.rows.size()));
        for (const auto& row : res.rows) {
            for (const auto& val : row) {
                out.push_back(val.isNull() ? 1 : 0);
                if (!val.isNull()) {
                    switch (val.getType()) {
                        case TypeId::Integer:
                            writeInt32(out, val.getAsInteger());
                            break;
                        case TypeId::Boolean:
                            out.push_back(val.getAsBoolean() ? 1 : 0);
                            break;
                        case TypeId::Varchar:
                            writeString(out, val.getAsVarchar());
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    }
    return out;
}

ResponseMessage WireProtocol::deserializeResponse(const std::string& data) {
    if (data.size() < 2) throw std::runtime_error("Invalid response payload size");
    ResponseMessage res;
    res.version = static_cast<uint8_t>(data[0]);
    res.type = static_cast<MessageType>(static_cast<uint8_t>(data[1]));
    size_t offset = 2;
    res.request_id = readInt32(data, offset);
    if (offset >= data.size()) throw std::runtime_error("Invalid bool");
    res.success = data[offset++] != 0;
    
    if (!res.success) {
        res.error_message = readString(data, offset);
    } else {
        int32_t num_cols = readInt32(data, offset);
        for (int32_t i = 0; i < num_cols; ++i) {
            ColumnMetadata col;
            col.name = readString(data, offset);
            if (offset >= data.size()) throw std::runtime_error("Invalid type id");
            col.type = static_cast<TypeId>(data[offset++]);
            res.columns.push_back(col);
        }
        int32_t num_rows = readInt32(data, offset);
        for (int32_t i = 0; i < num_rows; ++i) {
            std::vector<Value> row;
            for (int32_t j = 0; j < num_cols; ++j) {
                if (offset >= data.size()) throw std::runtime_error("Invalid null flag");
                bool is_null = data[offset++] != 0;
                if (is_null) {
                    row.emplace_back(Value());
                } else {
                    switch (res.columns[j].type) {
                        case TypeId::Integer:
                            row.emplace_back(Value(readInt32(data, offset)));
                            break;
                        case TypeId::Boolean:
                            if (offset >= data.size()) throw std::runtime_error("Invalid bool flag");
                            row.emplace_back(Value(data[offset++] != 0));
                            break;
                        case TypeId::Varchar:
                            row.emplace_back(Value(readString(data, offset)));
                            break;
                        default:
                            row.emplace_back(Value());
                            break;
                    }
                }
            }
            res.rows.push_back(std::move(row));
        }
    }
    return res;
}

} // namespace hamdb::server
