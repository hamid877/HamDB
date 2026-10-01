#include "server/wire_protocol.hpp"

namespace hamdb::server {

std::string WireProtocol::serializeRequest(const RequestMessage& req) {
    std::string out;
    out.push_back(static_cast<char>(req.version));
    out.push_back(static_cast<char>(req.type));
    out.append(req.query);
    return out;
}

RequestMessage WireProtocol::deserializeRequest(const std::string& data) {
    if (data.size() < 2) {
        throw std::runtime_error("Invalid request payload size");
    }
    RequestMessage req;
    req.version = static_cast<uint8_t>(data[0]);
    req.type = static_cast<MessageType>(static_cast<uint8_t>(data[1]));
    req.query = data.substr(2);
    return req;
}

std::string WireProtocol::serializeResponse(const ResponseMessage& res) {
    std::string out;
    out.push_back(static_cast<char>(res.version));
    out.push_back(static_cast<char>(res.type));
    out.append(res.data);
    return out;
}

ResponseMessage WireProtocol::deserializeResponse(const std::string& data) {
    if (data.size() < 2) {
        throw std::runtime_error("Invalid response payload size");
    }
    ResponseMessage res;
    res.version = static_cast<uint8_t>(data[0]);
    res.type = static_cast<MessageType>(static_cast<uint8_t>(data[1]));
    res.data = data.substr(2);
    return res;
}

} // namespace hamdb::server
