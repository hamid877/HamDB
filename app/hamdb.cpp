#include "shell/shell.hpp"
#include "shell/repl.hpp"
#include "shell/script_executor.hpp"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::string db_name = "hamdb.db";
    std::string script_path = "";
    
    if (argc == 2) {
        std::string arg = argv[1];
        if (arg.ends_with(".sql")) {
            script_path = arg;
        } else {
            db_name = arg;
        }
    } else if (argc >= 3) {
        db_name = argv[1];
        script_path = argv[2];
    }
    
    hamdb::shell::Shell shell(db_name);
    
    if (!script_path.empty()) {
        bool success = hamdb::shell::ScriptExecutor::execute(script_path, shell, std::cout);
        return success ? 0 : 1;
    }
    
    std::cout << "Welcome to HamDB v0.3.0-dev\n";
    std::cout << "Connected to database: " << db_name << "\n";
    std::cout << "Type \".help\" for instructions.\n";
    
    hamdb::shell::Repl repl(shell);
    
    repl.run(std::cin, std::cout);
    
    return 0;
}
