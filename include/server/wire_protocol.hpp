#pragma once

#include <string>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include "executor/value.hpp"

namespace hamdb::server {

enum class MessageType : uint8_t {
    QueryRequest = 1,
    QueryResponse = 2,
    ErrorResponse = 3
};

struct ColumnMetadata {
    std::string name;
    TypeId type;
};

struct RequestMessage {
    uint8_t version = 1;
    MessageType type = MessageType::QueryRequest;
    int32_t request_id = 0;
    std::string query;
};

struct ResponseMessage {
    uint8_t version = 1;
    MessageType type = MessageType::QueryResponse;
    int32_t request_id = 0;
    bool success = true;
    std::string error_message;
    std::vector<ColumnMetadata> columns;
    std::vector<std::vector<Value>> rows;
};

class WireProtocol {
public:
    static std::string serializeRequest(const RequestMessage& req);
    static RequestMessage deserializeRequest(const std::string& data);
    
    static std::string serializeResponse(const ResponseMessage& res);
    static ResponseMessage deserializeResponse(const std::string& data);
};

} // namespace hamdb::server
