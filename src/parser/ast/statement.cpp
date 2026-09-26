#include "parser/ast/statement.hpp"
#include <sstream>

namespace hamdb::ast {

std::string SelectStatement::toString() const {
    std::ostringstream oss;
    oss << "SELECT ";
    for (size_t i = 0; i < select_list.size(); ++i) {
        oss << select_list[i]->toString();
        if (i + 1 < select_list.size()) oss << ", ";
    }
    if (!table_name.empty()) {
        oss << " FROM " << table_name;
    }
    if (where_clause) {
        oss << " WHERE " << where_clause->toString();
    }
    if (!order_by.empty()) {
        oss << " ORDER BY ";
        for (size_t i = 0; i < order_by.size(); ++i) {
            oss << order_by[i].first->toString() << (order_by[i].second ? " ASC" : " DESC");
            if (i + 1 < order_by.size()) oss << ", ";
        }
    }
    if (limit) {
        oss << " LIMIT " << limit->toString();
    }
    if (offset) {
        oss << " OFFSET " << offset->toString();
    }
    return oss.str();
}

std::string InsertStatement::toString() const {
    std::ostringstream oss;
    oss << "INSERT INTO " << table_name << " VALUES ";
    for (size_t i = 0; i < values.size(); ++i) {
        oss << "(";
        for (size_t j = 0; j < values[i].size(); ++j) {
            oss << values[i][j]->toString();
            if (j + 1 < values[i].size()) oss << ", ";
        }
        oss << ")";
        if (i + 1 < values.size()) oss << ", ";
    }
    return oss.str();
}

std::string UpdateStatement::toString() const {
    std::ostringstream oss;
    oss << "UPDATE " << table_name << " SET ";
    for (size_t i = 0; i < set_clauses.size(); ++i) {
        oss << set_clauses[i].first << " = " << set_clauses[i].second->toString();
        if (i + 1 < set_clauses.size()) oss << ", ";
    }
    if (where_clause) {
        oss << " WHERE " << where_clause->toString();
    }
    return oss.str();
}

std::string DeleteStatement::toString() const {
    std::ostringstream oss;
    oss << "DELETE FROM " << table_name;
    if (where_clause) {
        oss << " WHERE " << where_clause->toString();
    }
    return oss.str();
}

std::string ValuesStatement::toString() const {
    std::ostringstream oss;
    oss << "VALUES ";
    for (size_t i = 0; i < values.size(); ++i) {
        oss << "(";
        for (size_t j = 0; j < values[i].size(); ++j) {
            oss << values[i][j]->toString();
            if (j + 1 < values[i].size()) oss << ", ";
        }
        oss << ")";
        if (i + 1 < values.size()) oss << ", ";
    }
    return oss.str();
}

} // namespace hamdb::ast
