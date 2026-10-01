#include "server/session.hpp"
#include "server/wire_protocol.hpp"
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

void Session::handleRequest(const std::string& payload) {
    if (!engine_ || !connection_) {
        return;
    }

    try {
        RequestMessage req = WireProtocol::deserializeRequest(payload);
        
        ResponseMessage res;
        res.version = 1;
        res.request_id = req.request_id;
        
        if (req.type != MessageType::QueryRequest) {
            res.type = MessageType::ErrorResponse;
            res.success = false;
            res.error_message = "Unsupported request type";
        } else {
            // Check for meta commands (though generally they shouldn't go through structured API, 
            // for backwards compatibility with some tests if needed)
            if (!req.query.empty() && req.query[0] == '.') {
                std::ostringstream out;
                engine_->executeMeta(req.query, out);
                res.type = MessageType::QueryResponse;
                res.success = true;
                res.columns.push_back({"output", TypeId::Varchar});
                res.rows.push_back({Value(out.str())});
            } else {
                shell::ExecutionResult exec_res = engine_->executeSQLStructured(req.query);
                if (exec_res.success) {
                    res.type = MessageType::QueryResponse;
                    res.success = true;
                    res.columns = exec_res.columns;
                    res.rows = exec_res.rows;
                } else {
                    res.type = MessageType::ErrorResponse;
                    res.success = false;
                    res.error_message = exec_res.error_message;
                }
            }
        }
        
        std::string res_payload = WireProtocol::serializeResponse(res);
        connection_->send(res_payload);

    } catch (const std::exception& e) {
        // Deserialization failure or other fatal error
        ResponseMessage res;
        res.version = 1;
        res.type = MessageType::ErrorResponse;
        res.request_id = 0; // Unknown
        res.success = false;
        res.error_message = e.what();
        
        connection_->send(WireProtocol::serializeResponse(res));
    }
}

} // namespace hamdb::server
