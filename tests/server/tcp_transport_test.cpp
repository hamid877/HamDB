#include <gtest/gtest.h>
#include "server/tcp_listener.hpp"
#include "server/tcp_connection.hpp"
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace hamdb::server;

class TcpTransportTest : public ::testing::Test {
protected:
    int connectToServer(const std::string& address, int port) {
        int sock = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return -1;
        
        struct sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        ::inet_pton(AF_INET, address.c_str(), &server_addr.sin_addr);
        
        if (::connect(sock, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
            ::close(sock);
            return -1;
        }
        return sock;
    }
};

TEST_F(TcpTransportTest, StartAndStopListener) {
    TcpListener listener("127.0.0.1", 15432);
    EXPECT_TRUE(listener.start());
    listener.stop();
}

TEST_F(TcpTransportTest, AcceptConnection) {
    TcpListener listener("127.0.0.1", 15433);
    ASSERT_TRUE(listener.start());
    
    std::thread client_thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int sock = connectToServer("127.0.0.1", 15433);
        EXPECT_GE(sock, 0);
        if (sock >= 0) ::close(sock);
    });
    
    auto conn = listener.acceptConnection();
    ASSERT_NE(conn, nullptr);
    
    client_thread.join();
    listener.stop();
}

TEST_F(TcpTransportTest, SendAndReceive) {
    TcpListener listener("127.0.0.1", 15434);
    ASSERT_TRUE(listener.start());
    
    std::string test_msg = "Hello HamDB\n";
    
    std::thread client_thread([this, &test_msg]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int sock = connectToServer("127.0.0.1", 15434);
        ASSERT_GE(sock, 0);
        
        ::send(sock, test_msg.data(), test_msg.size(), 0);
        
        char buffer[1024];
        ssize_t bytes = ::recv(sock, buffer, sizeof(buffer), 0);
        EXPECT_GT(bytes, 0);
        if (bytes > 0) {
            std::string resp(buffer, bytes);
            EXPECT_EQ(resp, "OK\n");
        }
        ::close(sock);
    });
    
    auto conn = listener.acceptConnection();
    ASSERT_NE(conn, nullptr);
    
    std::string received = conn->receive();
    EXPECT_EQ(received, test_msg);
    
    conn->send("OK\n");
    conn->close();
    
    client_thread.join();
    listener.stop();
}

TEST_F(TcpTransportTest, ClientDisconnect) {
    TcpListener listener("127.0.0.1", 15435);
    ASSERT_TRUE(listener.start());
    
    std::thread client_thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int sock = connectToServer("127.0.0.1", 15435);
        ASSERT_GE(sock, 0);
        ::close(sock);
    });
    
    auto conn = listener.acceptConnection();
    ASSERT_NE(conn, nullptr);
    
    std::string received = conn->receive();
    EXPECT_EQ(received, "");
    
    client_thread.join();
    listener.stop();
}
