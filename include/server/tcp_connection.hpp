#pragma once
#include "server/connection.hpp"
#include <string>

namespace hamdb::server {

constexpr size_t kMaxFrameSize = 16 * 1024 * 1024;

/**
 * @brief TCP implementation of the Connection interface with length-prefixed framing.
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
    std::string read_buffer_;
};

} // namespace hamdb::server
