#pragma once

#include <string>
#include <iostream>

namespace hamdb::shell {

class Shell;

class MetaCommands {
public:
    static void execute(const std::string& cmd, Shell& shell, std::ostream& out);
};

} // namespace hamdb::shell
