#include "server/server.hpp"
#include "shell/shell.hpp"
#include <algorithm>

namespace hamdb::server {

Server::Server(const std::string& db_name) {
    // For now, Shell initializes the entire engine and its dependencies.
    // By keeping one instance here, the Server avoids duplicating SQL/storage logic.
    engine_ = std::make_unique<shell::Shell>(db_name);
}

Server::~Server() {
    shutdown();
}

std::shared_ptr<Session> Server::createSession(std::shared_ptr<Connection> connection) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    if (!running_) {
        return nullptr;
    }
    auto session = std::make_shared<Session>(next_session_id_++, std::move(connection), engine_.get());
    sessions_.push_back(session);
    return session;
}

void Server::closeSession(int session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = std::find_if(sessions_.begin(), sessions_.end(),
                           [session_id](const std::shared_ptr<Session>& s) {
                               return s->getId() == session_id;
                           });
    
    if (it != sessions_.end()) {
        // Calling erase removes the shared_ptr from the vector, and if it's the last reference,
        // it triggers the ~Session() destructor, closing the Connection via RAII.
        sessions_.erase(it);
    }
}

void Server::shutdown() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    running_ = false;
    
    // Clear sessions to properly close all active connections before the engine gets destroyed.
    sessions_.clear();
    
    // RAII will destroy engine_ and cleanly release disk locks and flush buffers.
}

} // namespace hamdb::server
