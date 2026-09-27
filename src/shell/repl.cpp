#include "shell/repl.hpp"
#include <string>
#include <algorithm>

namespace hamdb::shell {

Repl::Repl(Shell& shell) : shell_(shell) {}

void Repl::run(std::istream& in, std::ostream& out) {
    std::string statement;
    std::string line;
    
    while (true) {
        if (statement.empty()) {
            out << "hamdb> ";
        } else {
            out << "....> ";
        }
        
        if (!std::getline(in, line)) {
            break; // EOF
        }
        
        // Trim leading spaces for meta command check
        size_t first_non_space = line.find_first_not_of(" \t");
        
        if (statement.empty() && first_non_space != std::string::npos && line[first_non_space] == '.') {
            std::string cmd = line.substr(first_non_space);
            if (cmd == ".exit" || cmd == ".quit") {
                break;
            }
            shell_.executeMeta(cmd, out);
            continue;
        }
        
        if (first_non_space == std::string::npos) {
            continue; // Empty line
        }

        statement += line + " ";
        
        if (!statement.empty() && statement.find(';') != std::string::npos) {
            // executeSQL
            shell_.executeSQL(statement, out);
            statement.clear();
        }
    }
}

} // namespace hamdb::shell
