#include <gtest/gtest.h>
#include "server/wire_protocol.hpp"

using namespace hamdb::server;

TEST(WireProtocolTest, SerializeDeserializeRequest) {
    RequestMessage req;
    req.version = 1;
    req.type = MessageType::QueryRequest;
    req.query = "SELECT * FROM test;";
    
    std::string payload = WireProtocol::serializeRequest(req);
    EXPECT_EQ(payload.size(), 2 + req.query.size());
    
    RequestMessage out = WireProtocol::deserializeRequest(payload);
    EXPECT_EQ(out.version, 1);
    EXPECT_EQ(out.type, MessageType::QueryRequest);
    EXPECT_EQ(out.query, "SELECT * FROM test;");
}

TEST(WireProtocolTest, SerializeDeserializeResponse) {
    ResponseMessage res;
    res.version = 2;
    res.type = MessageType::QueryResponse;
    res.data = "response data";
    
    std::string payload = WireProtocol::serializeResponse(res);
    EXPECT_EQ(payload.size(), 2 + res.data.size());
    
    ResponseMessage out = WireProtocol::deserializeResponse(payload);
    EXPECT_EQ(out.version, 2);
    EXPECT_EQ(out.type, MessageType::QueryResponse);
    EXPECT_EQ(out.data, "response data");
}

TEST(WireProtocolTest, InvalidPayloadSize) {
    EXPECT_THROW(WireProtocol::deserializeRequest("A"), std::runtime_error);
    EXPECT_THROW(WireProtocol::deserializeResponse("A"), std::runtime_error);
}
