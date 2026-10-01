#include "server/tcp_listener.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace hamdb::server {

TcpListener::TcpListener(const std::string& address, int port)
    : address_(address), port_(port) {
}

TcpListener::~TcpListener() {
    stop();
}

bool TcpListener::start() {
    if (running_) {
        return true;
    }

    server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        return false;
    }

    // Allow port reuse
    int opt = 1;
    if (::setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        stop();
        return false;
    }

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    if (::inet_pton(AF_INET, address_.c_str(), &server_addr.sin_addr) <= 0) {
        stop();
        return false;
    }

    if (::bind(server_fd_, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        stop();
        return false;
    }

    if (::listen(server_fd_, SOMAXCONN) < 0) {
        stop();
        return false;
    }

    running_ = true;
    return true;
}

std::shared_ptr<TcpConnection> TcpListener::acceptConnection() {
    if (!running_ || server_fd_ < 0) {
        return nullptr;
    }

    struct sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = ::accept(server_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
    if (client_fd < 0) {
        return nullptr;
    }

    return std::make_shared<TcpConnection>(client_fd);
}

void TcpListener::stop() {
    running_ = false;
    if (server_fd_ >= 0) {
        ::shutdown(server_fd_, SHUT_RDWR);
        ::close(server_fd_);
        server_fd_ = -1;
    }
}

} // namespace hamdb::server
