#pragma once

#include <string>
#include <cstdint>
#include <stdexcept>

namespace hamdb::server {

enum class MessageType : uint8_t {
    QueryRequest = 1,
    QueryResponse = 2,
    ErrorResponse = 3
};

struct RequestMessage {
    uint8_t version = 1;
    MessageType type = MessageType::QueryRequest;
    std::string query;
};

struct ResponseMessage {
    uint8_t version = 1;
    MessageType type = MessageType::QueryResponse;
    std::string data;
};

class WireProtocol {
public:
    static std::string serializeRequest(const RequestMessage& req);
    static RequestMessage deserializeRequest(const std::string& data);
    
    static std::string serializeResponse(const ResponseMessage& res);
    static ResponseMessage deserializeResponse(const std::string& data);
};

} // namespace hamdb::server
