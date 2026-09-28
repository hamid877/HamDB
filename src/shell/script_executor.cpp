#include "shell/script_executor.hpp"
#include <fstream>
#include <sstream>
#include <string>

namespace hamdb::shell {

namespace {
struct SplitContext {
    std::vector<std::string> statements;
    std::string current;
    bool in_string = false;
    bool in_line_comment = false;
    bool in_block_comment = false;

    void processChar(const std::string& script, size_t& i) {
        char c = script[i];
        if (in_line_comment) {
            current += c;
            if (c == '\n') in_line_comment = false;
            return;
        }
        if (in_block_comment) {
            current += c;
            if (c == '*' && i + 1 < script.length() && script[i+1] == '/') {
                current += '/';
                i++;
                in_block_comment = false;
            }
            return;
        }
        if (in_string) {
            current += c;
            if (c == '\'') in_string = false;
            return;
        }
        
        if (c == '\'') {
            in_string = true;
            current += c;
        } else if (c == '-' && i + 1 < script.length() && script[i+1] == '-') {
            in_line_comment = true;
            current += c;
        } else if (c == '/' && i + 1 < script.length() && script[i+1] == '*') {
            in_block_comment = true;
            current += c;
        } else if (c == ';') {
            current += c;
            if (current.find_first_not_of(" \t\n\r;") != std::string::npos) {
                statements.push_back(current);
            }
            current.clear();
        } else {
            current += c;
        }
    }
};
} // namespace

std::vector<std::string> StatementSplitter::split(const std::string& script) {
    SplitContext ctx;
    for (size_t i = 0; i < script.length(); ++i) {
        ctx.processChar(script, i);
    }
    
    if (ctx.current.find_first_not_of(" \t\n\r;") != std::string::npos) {
        ctx.statements.push_back(ctx.current);
    }
    
    return ctx.statements;
}

bool ScriptExecutor::execute(const std::string& filepath, Shell& shell, std::ostream& out) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        out << "Error: Cannot open script file " << filepath << "\n";
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string script = buffer.str();
    
    auto statements = StatementSplitter::split(script);
    int stmt_num = 1;
    for (const auto& stmt : statements) {
        std::stringstream local_out;
        shell.executeSQL(stmt, local_out);
        std::string output = local_out.str();
        
        if (output.starts_with("Error:")) {
            out << "Execution failed at statement " << stmt_num << ":\n";
            out << stmt << "\n";
            out << output;
            return false;
        }
        
        out << output;
        stmt_num++;
    }
    
    out << "Script execution completed successfully. (" << (stmt_num - 1) << " statements)\n";
    return true;
}

} // namespace hamdb::shell
