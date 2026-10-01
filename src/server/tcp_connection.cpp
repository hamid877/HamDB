#include "server/tcp_connection.hpp"
#include <unistd.h>
#include <sys/socket.h>

namespace hamdb::server {

TcpConnection::TcpConnection(int fd) : fd_(fd) {
}

TcpConnection::~TcpConnection() {
    close();
}

std::string TcpConnection::receive() {
    if (is_closed_) {
        return "";
    }
    
    char buffer[4096];
    ssize_t bytes_read = ::recv(fd_, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read > 0) {
        return std::string(buffer, bytes_read);
    } else {
        // EOF or error
        close();
        return "";
    }
}

void TcpConnection::send(const std::string& data) {
    if (is_closed_ || data.empty()) {
        return;
    }
    
    size_t total_sent = 0;
    while (total_sent < data.size()) {
        ssize_t sent = ::send(fd_, data.data() + total_sent, data.size() - total_sent, 0);
        if (sent <= 0) {
            close();
            break;
        }
        total_sent += sent;
    }
}

void TcpConnection::close() {
    if (!is_closed_) {
        ::close(fd_);
        is_closed_ = true;
    }
}

} // namespace hamdb::server
