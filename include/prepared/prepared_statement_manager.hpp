#pragma once

#include "prepared/prepared_statement.hpp"
#include <unordered_map>
#include <string>
#include <memory>

namespace hamdb {

class PreparedStatementManager {
public:
    void addStatement(std::unique_ptr<PreparedStatement> stmt);
    PreparedStatement* getStatement(const std::string& name) const;
    void removeStatement(const std::string& name);
    void clear();
    [[nodiscard]] std::size_t getStatementCount() const { return statements_.size(); }

private:
    std::unordered_map<std::string, std::unique_ptr<PreparedStatement>> statements_;
};

} // namespace hamdb
