#pragma once

#include "shell/shell.hpp"
#include <iostream>

namespace hamdb::shell {

class Repl {
public:
    explicit Repl(Shell& shell);

    void run(std::istream& in, std::ostream& out);

private:
    Shell& shell_;
};

} // namespace hamdb::shell
