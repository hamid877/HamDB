#include "server/session.hpp"
#include "shell/shell.hpp"
#include <sstream>
#include <iostream>

namespace hamdb::server {

Session::Session(int session_id, std::shared_ptr<Connection> connection, shell::Shell* engine)
    : session_id_(session_id), connection_(std::move(connection)), engine_(engine) {
}

Session::~Session() {
    if (connection_) {
        connection_->close();
    }
}

void Session::handleRequest(const std::string& query) {
    if (!engine_ || !connection_) {
        return;
    }

    std::ostringstream response_stream;

    // Handle meta commands vs standard SQL.
    if (!query.empty() && query[0] == '.') {
        engine_->executeMeta(query, response_stream);
    } else {
        engine_->executeSQL(query, response_stream);
    }

    std::string response = response_stream.str();
    if (response.empty()) {
        response = "OK\n";
    }

    connection_->send(response);
}

} // namespace hamdb::server
