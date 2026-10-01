#pragma once

#include "server/session.hpp"
#include <memory>
#include <vector>
#include <string>
#include <mutex>

namespace hamdb::shell {
class Shell;
}

namespace hamdb::server {

/**
 * @brief Top-level server architecture.
 *
 * Owns the database engine instance (delegated to Shell for now) and manages client sessions.
 * Follows RAII for lifecycle management and safe shutdown.
 */
class Server {
public:
    /**
     * @brief Constructs a new Server instance.
     * @param db_name The path/name of the database to operate on.
     */
    explicit Server(const std::string& db_name);
    
    ~Server();

    // Non-copyable
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    /**
     * @brief Creates a new session for a connected client.
     * @param connection The client's connection transport.
     * @return std::shared_ptr<Session> The created session.
     */
    std::shared_ptr<Session> createSession(std::shared_ptr<Connection> connection);

    /**
     * @brief Removes and cleans up a session.
     * @param session_id The ID of the session to remove.
     */
    void closeSession(int session_id);

    /**
     * @brief Shuts down the server, closing all sessions and flushing the engine.
     */
    void shutdown();

private:
    std::unique_ptr<shell::Shell> engine_;
    std::vector<std::shared_ptr<Session>> sessions_;
    std::mutex sessions_mutex_;
    int next_session_id_ = 1;
    bool running_ = true;
};

} // namespace hamdb::server
