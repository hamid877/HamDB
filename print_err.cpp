#include <iostream>
#include "server/server.hpp"
#include "server/session.hpp"
#include "server/wire_protocol.hpp"
#include "server/connection.hpp"
#include "shell/shell.hpp"
#include "catalog/catalog_manager.hpp"
#include "storage/table_heap.hpp"
#include "storage/slotted_page.hpp"

using namespace hamdb;
using namespace hamdb::server;

class MockConn : public Connection {
public:
    std::string receive() override { return ""; }
    void send(const std::string& data) override { last_sent = data; }
    void close() override {}
    std::string last_sent;
};

int main() {
    Server srv("test.db");
    auto conn = std::make_shared<MockConn>();
    auto session = srv.createSession(conn);
    
    Schema schema({
        Column("id", ColumnType::Integer),
        Column("name", ColumnType::Varchar)
    });
    TableInfo* info;
    (void)srv.getEngine()->getCatalog()->createTable("users", schema, info);
    
    auto bpm = srv.getEngine()->getBufferPoolManager();
    {
        WritePageGuard guard;
        (void)bpm->fetchPageWrite(info->getHeapRootPage(), guard);
        auto& page_mut = guard.pageMut();
        page_mut.header() = PageHeader(info->getHeapRootPage(), PageType::Table);
        SlottedPage sp(page_mut);
        (void)sp.initialize();
    }
    (void)bpm->flushAllPages();
    
    RequestMessage req; req.version=1; req.type=MessageType::QueryRequest; req.request_id=1; req.query="INSERT INTO users VALUES (1, 'Alice'), (2, 'Bob');";
    session->handleRequest(WireProtocol::serializeRequest(req));
    auto res = WireProtocol::deserializeResponse(conn->last_sent);
    std::cout << "INSERT success: " << res.success << " ROWS: " << res.rows.size() << " ERROR: " << res.error_message << "\n";
    if (res.rows.size() > 0 && res.rows[0].size() > 0) {
        std::cout << "INSERT returned count: " << res.rows[0][0].getAsInteger() << "\n";
    }

    req.request_id=2; req.query="SELECT id, name FROM users;";
    session->handleRequest(WireProtocol::serializeRequest(req));
    res = WireProtocol::deserializeResponse(conn->last_sent);
    std::cout << "SELECT success: " << res.success << " ROWS: " << res.rows.size() << " ERROR: " << res.error_message << "\n";
    return 0;
}
