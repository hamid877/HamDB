#pragma once
#include "server/tcp_connection.hpp"
#include <memory>
#include <string>

namespace hamdb::server {

/**
 * @brief TCP listener for accepting incoming client connections.
 */
class TcpListener {
public:
    TcpListener(const std::string& address, int port);
    ~TcpListener();

    // Non-copyable
    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;

    /**
     * @brief Binds the socket and starts listening for connections.
     * @return true if successful, false otherwise.
     */
    bool start();

    /**
     * @brief Blocks and accepts a new client connection.
     * @return std::shared_ptr<TcpConnection> representing the accepted client, or nullptr on failure/shutdown.
     */
    std::shared_ptr<TcpConnection> acceptConnection();

    /**
     * @brief Shuts down the listener.
     */
    void stop();

private:
    int server_fd_ = -1;
    std::string address_;
    int port_;
    bool running_ = false;
};

} // namespace hamdb::server
