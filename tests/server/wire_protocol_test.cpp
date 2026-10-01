#include <gtest/gtest.h>
#include "server/wire_protocol.hpp"

using namespace hamdb;
using namespace hamdb::server;

TEST(WireProtocolTest, SerializeDeserializeRequest) {
    RequestMessage req;
    req.version = 1;
    req.type = MessageType::QueryRequest;
    req.request_id = 42;
    req.query = "SELECT * FROM test;";
    
    std::string payload = WireProtocol::serializeRequest(req);
    
    RequestMessage out = WireProtocol::deserializeRequest(payload);
    EXPECT_EQ(out.version, 1);
    EXPECT_EQ(out.type, MessageType::QueryRequest);
    EXPECT_EQ(out.request_id, 42);
    EXPECT_EQ(out.query, "SELECT * FROM test;");
}

TEST(WireProtocolTest, SerializeDeserializeResponseSuccess) {
    ResponseMessage res;
    res.version = 1;
    res.type = MessageType::QueryResponse;
    res.request_id = 42;
    res.success = true;
    
    ColumnMetadata col1{"id", TypeId::Integer};
    ColumnMetadata col2{"name", TypeId::Varchar};
    res.columns.push_back(col1);
    res.columns.push_back(col2);
    
    std::vector<Value> row1 = {Value(1), Value("Alice")};
    std::vector<Value> row2 = {Value(2), Value()}; // name is null
    res.rows.push_back(row1);
    res.rows.push_back(row2);
    
    std::string payload = WireProtocol::serializeResponse(res);
    
    ResponseMessage out = WireProtocol::deserializeResponse(payload);
    EXPECT_EQ(out.version, 1);
    EXPECT_EQ(out.type, MessageType::QueryResponse);
    EXPECT_EQ(out.request_id, 42);
    EXPECT_TRUE(out.success);
    
    ASSERT_EQ(out.columns.size(), 2);
    EXPECT_EQ(out.columns[0].name, "id");
    EXPECT_EQ(out.columns[0].type, TypeId::Integer);
    EXPECT_EQ(out.columns[1].name, "name");
    EXPECT_EQ(out.columns[1].type, TypeId::Varchar);
    
    ASSERT_EQ(out.rows.size(), 2);
    EXPECT_EQ(out.rows[0][0].getAsInteger(), 1);
    EXPECT_EQ(out.rows[0][1].getAsVarchar(), "Alice");
    
    EXPECT_EQ(out.rows[1][0].getAsInteger(), 2);
    EXPECT_TRUE(out.rows[1][1].isNull());
}

TEST(WireProtocolTest, SerializeDeserializeResponseError) {
    ResponseMessage res;
    res.version = 1;
    res.type = MessageType::ErrorResponse; // or QueryResponse with success=false
    res.request_id = 99;
    res.success = false;
    res.error_message = "Syntax error";
    
    std::string payload = WireProtocol::serializeResponse(res);
    
    ResponseMessage out = WireProtocol::deserializeResponse(payload);
    EXPECT_EQ(out.request_id, 99);
    EXPECT_FALSE(out.success);
    EXPECT_EQ(out.error_message, "Syntax error");
    EXPECT_TRUE(out.columns.empty());
    EXPECT_TRUE(out.rows.empty());
}

TEST(WireProtocolTest, InvalidPayloadSize) {
    EXPECT_THROW(WireProtocol::deserializeRequest("A"), std::runtime_error);
    EXPECT_THROW(WireProtocol::deserializeResponse("A"), std::runtime_error);
}
