#pragma once

#include "shell/shell.hpp"
#include <string>
#include <vector>
#include <iostream>

namespace hamdb::shell {

class StatementSplitter {
public:
    static std::vector<std::string> split(const std::string& script);
};

class ScriptExecutor {
public:
    static bool execute(const std::string& filepath, Shell& shell, std::ostream& out);
};

} // namespace hamdb::shell
