#include "shell/table_printer.hpp"
#include <iomanip>
#include <algorithm>

namespace hamdb::shell {

void TablePrinter::setSchema(const std::vector<std::string>& columns) {
    columns_ = columns;
}

void TablePrinter::addRow(const std::vector<std::string>& row) {
    rows_.push_back(row);
}

void TablePrinter::clear() {
    columns_.clear();
    rows_.clear();
}

void TablePrinter::print(std::ostream& os) const {
    if (columns_.empty()) return;

    std::vector<size_t> widths(columns_.size(), 0);
    for (size_t i = 0; i < columns_.size(); ++i) {
        widths[i] = std::max(widths[i], columns_[i].length());
    }
    for (const auto& row : rows_) {
        for (size_t i = 0; i < row.size() && i < widths.size(); ++i) {
            widths[i] = std::max(widths[i], row[i].length());
        }
    }

    auto print_separator = [&]() {
        os << "+";
        for (size_t w : widths) {
            os << std::string(w + 2, '-') << "+";
        }
        os << "\n";
    };

    print_separator();
    
    os << "|";
    for (size_t i = 0; i < columns_.size(); ++i) {
        os << " " << std::left << std::setw(static_cast<int>(widths[i])) << columns_[i] << " |";
    }
    os << "\n";
    
    print_separator();
    
    for (const auto& row : rows_) {
        os << "|";
        for (size_t i = 0; i < row.size() && i < widths.size(); ++i) {
            os << " " << std::left << std::setw(static_cast<int>(widths[i])) << row[i] << " |";
        }
        os << "\n";
    }
    
    if (!rows_.empty()) {
        print_separator();
    }
}

void TablePrinter::printRowCount(std::ostream& os, size_t count) {
    os << "(" << count << " row" << (count == 1 ? "" : "s") << ")\n";
}

} // namespace hamdb::shell
