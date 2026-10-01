#include <gtest/gtest.h>
#include "server/server.hpp"
#include "server/session.hpp"
#include "server/wire_protocol.hpp"
#include "shell/shell.hpp"
#include "catalog/catalog_manager.hpp"
#include "storage/database_metadata.hpp"
#include "storage/table_heap.hpp"
#include "storage/slotted_page.hpp"
#include <fstream>

#include <filesystem>
#include <string>

using namespace hamdb;
using namespace hamdb::server;

class MockClientConnection : public Connection {
public:
    std::string receive() override {
        return "";
    }

    void send(const std::string& data) override {
        last_sent = data;
    }

    void close() override {
        is_closed = true;
    }

    std::string last_sent;
    bool is_closed = false;
};

class QueryExecutionTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_path = "test_integration.db";
        if (std::filesystem::exists(db_path)) {
            std::filesystem::remove(db_path);
        }
        srv = std::make_unique<Server>(db_path);
        conn = std::make_shared<MockClientConnection>();
        session = srv->createSession(conn);
    }

    void TearDown() override {
        session.reset();
        srv->shutdown();
        if (std::filesystem::exists(db_path)) {
            std::filesystem::remove(db_path);
        }
    }

    ResponseMessage executeQuery(const std::string& query, int32_t req_id) {
        RequestMessage req;
        req.version = 1;
        req.type = MessageType::QueryRequest;
        req.request_id = req_id;
        req.query = query;
        
        session->handleRequest(WireProtocol::serializeRequest(req));
        return WireProtocol::deserializeResponse(conn->last_sent);
    }

    std::string db_path;
    std::unique_ptr<Server> srv;
    std::shared_ptr<MockClientConnection> conn;
    std::shared_ptr<Session> session;
};

TEST_F(QueryExecutionTest, ExecuteCreateInsertSelect) {
    // CREATE TABLE using Catalog (since Parser lacks CREATE TABLE)
    Schema schema({
        Column("id", ColumnType::Integer),
        Column("name", ColumnType::Varchar)
    });
    TableInfo* info;
    EXPECT_EQ(srv->getEngine()->getCatalog()->createTable("users", schema, info), Status::Ok);
    
    // Initialize TableHeap via BPM
    auto bpm = srv->getEngine()->getBufferPoolManager();
    {
        WritePageGuard guard;
        EXPECT_EQ(bpm->fetchPageWrite(info->getHeapRootPage(), guard), Status::Ok);
        auto& page_mut = guard.pageMut();
        page_mut.header() = PageHeader(info->getHeapRootPage(), PageType::Table);
        SlottedPage sp(page_mut);
        EXPECT_EQ(sp.initialize(), Status::Ok);
        guard.markDirty();
    }
    (void)bpm->flushAllPages();
    
    // Fix database metadata page_count
    std::fstream stream(db_path, std::ios::in | std::ios::out | std::ios::binary);
    std::array<std::uint8_t, DatabaseMetadata::kSize> meta_buf{};
    stream.read(reinterpret_cast<char*>(meta_buf.data()), DatabaseMetadata::kSize);
    DatabaseMetadata meta;
    meta.deserialize(meta_buf);
    if (meta.page_count < 3) {
        meta.page_count = 3;
        meta.serialize(meta_buf);
        stream.seekp(0);
        stream.write(reinterpret_cast<const char*>(meta_buf.data()), DatabaseMetadata::kSize);
    }
    stream.close();
    
    // INSERT
    auto res = executeQuery("INSERT INTO users VALUES (1, 'Alice'), (2, 'Bob');", 2);
    EXPECT_TRUE(res.success) << res.error_message;
    EXPECT_EQ(res.request_id, 2);
    
    // SELECT
    res = executeQuery("SELECT id, name FROM users;", 3);
    EXPECT_TRUE(res.success);
    EXPECT_EQ(res.request_id, 3);
    
    // Verify schema
    ASSERT_EQ(res.columns.size(), 2);
    EXPECT_EQ(res.columns[0].name, "id");
    EXPECT_EQ(res.columns[0].type, TypeId::Integer);
    EXPECT_EQ(res.columns[1].name, "name");
    EXPECT_EQ(res.columns[1].type, TypeId::Varchar);
    
    // Verify rows
    ASSERT_EQ(res.rows.size(), 2);
    EXPECT_EQ(res.rows[0][0].getAsInteger(), 1);
    EXPECT_EQ(res.rows[0][1].getAsVarchar(), "Alice");
    EXPECT_EQ(res.rows[1][0].getAsInteger(), 2);
    EXPECT_EQ(res.rows[1][1].getAsVarchar(), "Bob");
}

TEST_F(QueryExecutionTest, ExecuteInvalidQuery) {
    auto res = executeQuery("SELECT * FROM non_existent_table;", 4);
    EXPECT_FALSE(res.success);
    EXPECT_EQ(res.request_id, 4);
    EXPECT_FALSE(res.error_message.empty());
    EXPECT_TRUE(res.columns.empty());
    EXPECT_TRUE(res.rows.empty());
}
