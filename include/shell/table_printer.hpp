#pragma once

#include <vector>
#include <string>
#include <iostream>
#include "catalog/schema.hpp"
#include "storage/tuple.hpp"
#include "executor/value.hpp"

namespace hamdb::shell {

class TablePrinter {
public:
    TablePrinter() = default;

    void setSchema(const std::vector<std::string>& columns);
    void addRow(const std::vector<std::string>& row);
    void print(std::ostream& os) const;
    static void printRowCount(std::ostream& os, size_t count);
    void clear();

private:
    std::vector<std::string> columns_;
    std::vector<std::vector<std::string>> rows_;
};

} // namespace hamdb::shell
