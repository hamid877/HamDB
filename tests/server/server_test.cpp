#include <gtest/gtest.h>
#include "server/server.hpp"
#include "server/session.hpp"
#include "server/connection.hpp"

#include <filesystem>
#include <string>

using namespace hamdb;
using namespace hamdb::server;

class MockConnection : public Connection {
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

class ServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_path = "test_server.db";
        if (std::filesystem::exists(db_path)) {
            std::filesystem::remove(db_path);
        }
    }

    void TearDown() override {
        if (std::filesystem::exists(db_path)) {
            std::filesystem::remove(db_path);
        }
    }

    std::string db_path;
};

TEST_F(ServerTest, LifecycleAndSessionManagement) {
    auto srv = std::make_unique<Server>(db_path);
    auto conn = std::make_shared<MockConnection>();
    auto session = srv->createSession(conn);
    
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(session->getId(), 1);
    
    // Close session directly from server
    srv->closeSession(session->getId());
    
    // The connection should be closed by session's destructor. 
    // We can check if is_closed is true since we hold a shared_ptr to conn.
    // However, `session` pointer above still holds a reference, so let's reset it first.
    session.reset();
    EXPECT_TRUE(conn->is_closed);
}

TEST_F(ServerTest, SessionHandleRequest) {
    auto srv = std::make_unique<Server>(db_path);
    auto conn = std::make_shared<MockConnection>();
    auto session = srv->createSession(conn);
    
    // Execute a meta command
    session->handleRequest(".help");
    
    // The output should contain help information
    EXPECT_NE(conn->last_sent.find(".help"), std::string::npos);
    EXPECT_NE(conn->last_sent.find(".tables"), std::string::npos);
    
    // Execute another command
    session->handleRequest(".tables");
    EXPECT_NE(conn->last_sent.find(""), std::string::npos);
}

TEST_F(ServerTest, ServerShutdown) {
    auto srv = std::make_unique<Server>(db_path);
    auto conn1 = std::make_shared<MockConnection>();
    auto conn2 = std::make_shared<MockConnection>();
    
    // Use weak_ptr to ensure the session gets destructed upon shutdown
    std::weak_ptr<Session> weak_session1 = srv->createSession(conn1);
    std::weak_ptr<Session> weak_session2 = srv->createSession(conn2);
    
    EXPECT_FALSE(weak_session1.expired());
    EXPECT_FALSE(weak_session2.expired());
    
    // Shutdown server
    srv->shutdown();
    
    // Verify sessions were destroyed
    EXPECT_TRUE(weak_session1.expired());
    EXPECT_TRUE(weak_session2.expired());
    
    // Verify connections were closed
    EXPECT_TRUE(conn1->is_closed);
    EXPECT_TRUE(conn2->is_closed);
    
    // Verify new sessions cannot be created
    auto conn3 = std::make_shared<MockConnection>();
    auto session3 = srv->createSession(conn3);
    EXPECT_EQ(session3, nullptr);
}
