#pragma once
#include "server/connection.hpp"
#include <string>

namespace hamdb::server {

/**
 * @brief TCP implementation of the Connection interface.
 */
class TcpConnection : public Connection {
public:
    explicit TcpConnection(int fd);
    ~TcpConnection() override;

    // Non-copyable
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    std::string receive() override;
    void send(const std::string& data) override;
    void close() override;

private:
    int fd_;
    bool is_closed_ = false;
};

} // namespace hamdb::server
