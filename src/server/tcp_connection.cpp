#include "server/tcp_connection.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <cstring>
#include <arpa/inet.h>

namespace hamdb::server {

TcpConnection::TcpConnection(int fd) : fd_(fd) {
}

TcpConnection::~TcpConnection() {
    close();
}

std::string TcpConnection::receive() {
    while (!is_closed_) {
        if (read_buffer_.size() >= 4) {
            uint32_t payload_len = 0;
            std::memcpy(&payload_len, read_buffer_.data(), 4);
            payload_len = ntohl(payload_len);
            
            if (payload_len > kMaxFrameSize) {
                close();
                return "";
            }
            
            if (read_buffer_.size() >= 4 + payload_len) {
                std::string payload = read_buffer_.substr(4, payload_len);
                read_buffer_.erase(0, 4 + payload_len);
                return payload;
            }
        }
        
        char buffer[4096];
        ssize_t bytes_read = ::recv(fd_, buffer, sizeof(buffer), 0);
        if (bytes_read > 0) {
            read_buffer_.append(buffer, bytes_read);
        } else {
            // EOF or error
            close();
            return "";
        }
    }
    return "";
}

void TcpConnection::send(const std::string& data) {
    if (is_closed_) {
        return;
    }
    
    if (data.size() > kMaxFrameSize) {
        close();
        return;
    }
    
    uint32_t payload_len = htonl(static_cast<uint32_t>(data.size()));
    std::string frame;
    frame.append(reinterpret_cast<const char*>(&payload_len), 4);
    frame.append(data);
    
    size_t total_sent = 0;
    while (total_sent < frame.size()) {
        ssize_t sent = ::send(fd_, frame.data() + total_sent, frame.size() - total_sent, 0);
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
