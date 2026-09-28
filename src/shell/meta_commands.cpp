#include "shell/meta_commands.hpp"
#include "shell/shell.hpp"
#include "shell/table_printer.hpp"
#include "shell/script_executor.hpp"
#include <sstream>

namespace hamdb::shell {

void MetaCommands::execute(const std::string& cmd, Shell& shell, std::ostream& out) {
    std::stringstream ss(cmd);
    std::string token;
    ss >> token;

    if (token == ".help") {
        out << ".help                  Show this message\n"
            << ".exit                  Exit this program\n"
            << ".quit                  Exit this program\n"
            << ".tables                List names of tables\n"
            << ".schema <table>        Show the CREATE TABLE statements\n"
            << ".describe <table>      Show columns and types\n"
            << ".indexes <table>       Show indexes of a table\n"
            << ".stats                 Show database statistics\n";
    } else if (token == ".tables") {
        auto tables = shell.getCatalog()->listTables();
        for (const auto& t : tables) {
            out << t << "\n";
        }
    } else if (token == ".schema") {
        std::string table_name;
        if (ss >> table_name) {
            TableInfo* info = nullptr;
            if (shell.getCatalog()->getTable(table_name, info) == Status::Ok) {
                out << "CREATE TABLE " << table_name << " (\n";
                const auto& schema = info->getSchema();
                for (size_t i = 0; i < schema.getColumnCount(); ++i) {
                    const auto& col = schema.getColumn(i);
                    out << "    " << col.getName() << " ";
                    switch(col.getType()) {
                        case ColumnType::Integer: out << "INTEGER"; break;
                        case ColumnType::Boolean: out << "BOOLEAN"; break;
                        case ColumnType::Varchar: out << "VARCHAR"; break;
                        case ColumnType::Float: out << "FLOAT"; break;
                    }
                    if (i + 1 < schema.getColumnCount()) {
                        out << ",";
                    }
                    out << "\n";
                }
                out << ");\n";
            } else {
                out << "Error: unknown table\n";
            }
        } else {
            out << "Usage: .schema <table>\n";
        }
    } else if (token == ".describe") {
        std::string table_name;
        if (ss >> table_name) {
            TableInfo* info = nullptr;
            if (shell.getCatalog()->getTable(table_name, info) == Status::Ok) {
                TablePrinter printer;
                printer.setSchema({"Column", "Type"});
                const auto& schema = info->getSchema();
                for (size_t i = 0; i < schema.getColumnCount(); ++i) {
                    const auto& col = schema.getColumn(i);
                    std::string type_str;
                    switch(col.getType()) {
                        case ColumnType::Integer: type_str = "INTEGER"; break;
                        case ColumnType::Boolean: type_str = "BOOLEAN"; break;
                        case ColumnType::Varchar: type_str = "VARCHAR"; break;
                        case ColumnType::Float: type_str = "FLOAT"; break;
                    }
                    printer.addRow({col.getName(), type_str});
                }
                printer.print(out);
            } else {
                out << "Error: unknown table\n";
            }
        } else {
            out << "Usage: .describe <table>\n";
        }
    } else if (token == ".indexes") {
        std::string table_name;
        if (ss >> table_name) {
            TableInfo* info = nullptr;
            if (shell.getCatalog()->getTable(table_name, info) == Status::Ok) {
                if (info->getIndexRootPage() != kInvalidPageId) {
                    out << "Index on " << table_name << " (Root Page: " << info->getIndexRootPage() << ")\n";
                } else {
                    out << "No indexes on " << table_name << "\n";
                }
            } else {
                out << "Error: unknown table\n";
            }
        } else {
            out << "Usage: .indexes <table>\n";
        }
    } else if (token == ".stats") {
        TablePrinter printer;
        printer.setSchema({"Metric", "Value"});
        
        auto cat = shell.getCatalog();
        auto bpm = shell.getBufferPoolManager();
        auto dm = shell.getDiskManager();
        auto tm = shell.getTransactionManager();
        auto pm = shell.getPreparedStatementManager();

        printer.addRow({"Tables", std::to_string(cat->listTables().size())});
        printer.addRow({"Buffer Pool Size", std::to_string(bpm->getPoolSize())});
        printer.addRow({"Disk Pages", std::to_string(dm->pageCount())});
        printer.addRow({"Active Transactions", std::to_string(tm->getActiveTxnCount())});
        printer.addRow({"Prepared Statements", std::to_string(pm->getStatementCount())});
        
        printer.print(out);
    } else if (token == ".read") {
        std::string filepath;
        if (ss >> filepath) {
            ScriptExecutor::execute(filepath, shell, out);
        } else {
            out << "Usage: .read <filepath>\n";
        }
    } else if (token == ".exit" || token == ".quit") {
        // Handled by repl
    } else {
        out << "Error: unknown command\n";
    }
}

} // namespace hamdb::shell
