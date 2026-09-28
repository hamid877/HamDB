#include "prepared/prepared_statement_manager.hpp"

namespace hamdb {

void PreparedStatementManager::addStatement(std::unique_ptr<PreparedStatement> stmt) {
    statements_[stmt->getName()] = std::move(stmt);
}

PreparedStatement* PreparedStatementManager::getStatement(const std::string& name) const {
    auto it = statements_.find(name);
    if (it != statements_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void PreparedStatementManager::removeStatement(const std::string& name) {
    statements_.erase(name);
}

void PreparedStatementManager::clear() {
    statements_.clear();
}

} // namespace hamdb
