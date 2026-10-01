#pragma once

#include "server/connection.hpp"
#include <memory>
#include <string>

namespace hamdb::shell {
class Shell; // Forward declaration
}

namespace hamdb::server {

/**
 * @brief Represents per-client database context.
 *
 * Handles execution of client requests by delegating to the database engine.
 */
class Session {
public:
    Session(int session_id, std::shared_ptr<Connection> connection, shell::Shell* engine);
    ~Session();

    /**
     * @brief Processes a single client request.
     * @param query The SQL query or command from the client.
     */
    void handleRequest(const std::string& query);

    int getId() const { return session_id_; }

    std::shared_ptr<Connection> getConnection() const { return connection_; }

private:
    int session_id_;
    std::shared_ptr<Connection> connection_;
    shell::Shell* engine_; // Temporary delegation target until Database facade is fully wired
};

} // namespace hamdb::server
