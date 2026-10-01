#pragma once

#include <string>
#include <memory>

namespace hamdb::server {

/**
 * @brief Represents client transport state.
 *
 * This is an interface for the underlying transport (e.g., TCP socket, local pipe, mock).
 * Future milestones will provide a TCP implementation.
 */
class Connection {
public:
    virtual ~Connection() = default;

    /**
     * @brief Reads a request from the client.
     */
    virtual std::string receive() = 0;

    /**
     * @brief Sends a response to the client.
     */
    virtual void send(const std::string& data) = 0;

    /**
     * @brief Closes the connection.
     */
    virtual void close() = 0;
};

} // namespace hamdb::server
