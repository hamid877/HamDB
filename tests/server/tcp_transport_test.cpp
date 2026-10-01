#include <gtest/gtest.h>
#include "server/tcp_listener.hpp"
#include "server/tcp_connection.hpp"
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

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

TEST_F(TcpTransportTest, SingleFrame) {
    TcpListener listener("127.0.0.1", 15434);
    ASSERT_TRUE(listener.start());
    
    std::thread client_thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int sock = connectToServer("127.0.0.1", 15434);
        ASSERT_GE(sock, 0);
        
        std::string payload = "Hello";
        uint32_t len = htonl(payload.size());
        ::send(sock, &len, 4, 0);
        ::send(sock, payload.data(), payload.size(), 0);
        
        ::close(sock);
    });
    
    auto conn = listener.acceptConnection();
    ASSERT_NE(conn, nullptr);
    
    std::string received = conn->receive();
    EXPECT_EQ(received, "Hello");
    
    client_thread.join();
    listener.stop();
}

TEST_F(TcpTransportTest, PartialReads) {
    TcpListener listener("127.0.0.1", 15435);
    ASSERT_TRUE(listener.start());
    
    std::thread client_thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int sock = connectToServer("127.0.0.1", 15435);
        ASSERT_GE(sock, 0);
        
        std::string payload = "Partial";
        uint32_t len = htonl(payload.size());
        std::string frame;
        frame.append(reinterpret_cast<char*>(&len), 4);
        frame.append(payload);
        
        for (char c : frame) {
            ::send(sock, &c, 1, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        ::close(sock);
    });
    
    auto conn = listener.acceptConnection();
    ASSERT_NE(conn, nullptr);
    
    EXPECT_EQ(conn->receive(), "Partial");
    
    client_thread.join();
    listener.stop();
}

TEST_F(TcpTransportTest, CombinedFrames) {
    TcpListener listener("127.0.0.1", 15436);
    ASSERT_TRUE(listener.start());
    
    std::thread client_thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int sock = connectToServer("127.0.0.1", 15436);
        ASSERT_GE(sock, 0);
        
        std::string payload1 = "Frame1";
        std::string payload2 = "Frame2";
        
        std::string frames;
        uint32_t len1 = htonl(payload1.size());
        frames.append(reinterpret_cast<char*>(&len1), 4);
        frames.append(payload1);
        
        uint32_t len2 = htonl(payload2.size());
        frames.append(reinterpret_cast<char*>(&len2), 4);
        frames.append(payload2);
        
        ::send(sock, frames.data(), frames.size(), 0);
        ::close(sock);
    });
    
    auto conn = listener.acceptConnection();
    ASSERT_NE(conn, nullptr);
    
    EXPECT_EQ(conn->receive(), "Frame1");
    EXPECT_EQ(conn->receive(), "Frame2");
    
    client_thread.join();
    listener.stop();
}

TEST_F(TcpTransportTest, EmptyPayload) {
    TcpListener listener("127.0.0.1", 15437);
    ASSERT_TRUE(listener.start());
    
    std::thread client_thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int sock = connectToServer("127.0.0.1", 15437);
        ASSERT_GE(sock, 0);
        
        uint32_t len = 0;
        ::send(sock, &len, 4, 0);
        ::close(sock);
    });
    
    auto conn = listener.acceptConnection();
    ASSERT_NE(conn, nullptr);
    
    EXPECT_EQ(conn->receive(), "");
    EXPECT_EQ(conn->receive(), "");
    
    client_thread.join();
    listener.stop();
}

TEST_F(TcpTransportTest, OversizedFrame) {
    TcpListener listener("127.0.0.1", 15438);
    ASSERT_TRUE(listener.start());
    
    std::thread client_thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int sock = connectToServer("127.0.0.1", 15438);
        ASSERT_GE(sock, 0);
        
        uint32_t len = htonl(17 * 1024 * 1024);
        ::send(sock, &len, 4, 0);
        ::close(sock);
    });
    
    auto conn = listener.acceptConnection();
    ASSERT_NE(conn, nullptr);
    
    EXPECT_EQ(conn->receive(), "");
    
    client_thread.join();
    listener.stop();
}

TEST_F(TcpTransportTest, TruncatedConnection) {
    TcpListener listener("127.0.0.1", 15439);
    ASSERT_TRUE(listener.start());
    
    std::thread client_thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int sock = connectToServer("127.0.0.1", 15439);
        ASSERT_GE(sock, 0);
        
        std::string payload = "Incomplete";
        uint32_t len = htonl(payload.size());
        
        ::send(sock, &len, 4, 0);
        ::send(sock, payload.data(), payload.size() / 2, 0);
        ::close(sock);
    });
    
    auto conn = listener.acceptConnection();
    ASSERT_NE(conn, nullptr);
    
    EXPECT_EQ(conn->receive(), "");
    
    client_thread.join();
    listener.stop();
}
