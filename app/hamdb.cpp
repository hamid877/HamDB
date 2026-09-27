#include "shell/shell.hpp"
#include "shell/repl.hpp"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::string db_name = "hamdb.db";
    if (argc > 1) {
        db_name = argv[1];
    }
    
    std::cout << "Welcome to HamDB v0.3.0-dev\n";
    std::cout << "Connected to database: " << db_name << "\n";
    std::cout << "Type \".help\" for instructions.\n";
    
    hamdb::shell::Shell shell(db_name);
    hamdb::shell::Repl repl(shell);
    
    repl.run(std::cin, std::cout);
    
    return 0;
}
