#include "shell/shell.hpp"
#include "shell/table_printer.hpp"
#include "parser/parser.hpp"
#include "binder/binder.hpp"
#include "planner/executor_factory.hpp"
#include "executor/column_value_expression.hpp"
#include "optimizer/constant_folding_rule.hpp"
#include "optimizer/predicate_pushdown_rule.hpp"
#include "optimizer/projection_pruning_rule.hpp"
#include "optimizer/index_scan_rule.hpp"
#include "optimizer/sort_limit_rule.hpp"
#include "planner/explain_plan.hpp"
#include "planner/plan_formatter.hpp"

#include <stdexcept>
#include <sstream>

namespace hamdb::shell {

Shell::Shell(const std::string& db_name) {
    initDB(db_name);
}

Shell::~Shell() = default;

void Shell::initDB(const std::string& db_name) {
    disk_manager_ = std::make_unique<DiskManager>(db_name);
    if (disk_manager_->openDatabase() != Status::Ok) {
        (void)disk_manager_->createDatabase();
        (void)disk_manager_->openDatabase();
    }
    
    bpm_ = std::make_unique<BufferPoolManager>(128, *disk_manager_);
    catalog_ = std::make_unique<CatalogManager>(bpm_.get());
    
    log_manager_ = std::make_unique<LogManager>();
    lock_manager_ = std::make_unique<LockManager>();
    mvcc_manager_ = std::make_unique<MvccManager>();
    txn_manager_ = std::make_unique<TransactionManager>();
    
    planner_ = std::make_unique<planner::Planner>(catalog_.get());
    physical_planner_ = std::make_unique<planner::PhysicalPlanner>(catalog_.get());
    
    optimizer_ = std::make_unique<optimizer::RuleExecutor>();
    optimizer_->addRule(std::make_unique<optimizer::PredicatePushdownRule>());
    optimizer_->addRule(std::make_unique<optimizer::ProjectionPruningRule>());
    optimizer_->addRule(std::make_unique<optimizer::ConstantFoldingRule>());
    optimizer_->addRule(std::make_unique<optimizer::IndexScanRule>(catalog_.get()));
    optimizer_->addRule(std::make_unique<optimizer::SortLimitRule>());
}

void Shell::executeMeta(const std::string& cmd, std::ostream& out) {
    std::stringstream ss(cmd);
    std::string token;
    ss >> token;

    if (token == ".help") {
        out << ".help                  Show this message\n"
            << ".exit                  Exit this program\n"
            << ".quit                  Exit this program\n"
            << ".tables                List names of tables\n"
            << ".schema <table>        Show the CREATE TABLE statements\n"
            << ".indexes <table>       Show indexes of a table\n";
    } else if (token == ".tables") {
        auto tables = catalog_->listTables();
        for (const auto& t : tables) {
            out << t << "\n";
        }
    } else if (token == ".schema") {
        std::string table_name;
        if (ss >> table_name) {
            TableInfo* info = nullptr;
            if (catalog_->getTable(table_name, info) == Status::Ok) {
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
                out << "Error: Table not found.\n";
            }
        } else {
            out << "Usage: .schema <table>\n";
        }
    } else if (token == ".indexes") {
        std::string table_name;
        if (ss >> table_name) {
            TableInfo* info = nullptr;
            if (catalog_->getTable(table_name, info) == Status::Ok) {
                if (info->getIndexRootPage() != kInvalidPageId) {
                    out << "Index on " << table_name << " (Root Page: " << info->getIndexRootPage() << ")\n";
                } else {
                    out << "No indexes on " << table_name << "\n";
                }
            } else {
                out << "Error: Table not found.\n";
            }
        } else {
            out << "Usage: .indexes <table>\n";
        }
    } else if (token == ".exit" || token == ".quit") {
        // Handled by repl
    } else {
        out << "Error: unknown command or invalid arguments:  " << cmd << "\n";
    }
}

void Shell::executeSQL(const std::string& query, std::ostream& out) {
    try {
        Parser parser(query);
        auto ast = parser.parseStatement();
        
        bool is_explain = false;
        bool is_analyze = false;
        ast::Statement* inner_stmt = ast.get();
        if (auto* explain_stmt = dynamic_cast<ast::ExplainStatement*>(ast.get())) {
            is_explain = true;
            is_analyze = explain_stmt->analyze;
            inner_stmt = explain_stmt->statement.get();
        }
        
        binder::Binder binder(catalog_.get());
        auto bound_stmt = binder.bind(*inner_stmt);
        
        planner::ExplainPlan explain;
        
        auto logical_plan = planner_->plan(std::move(bound_stmt));
        if (is_explain) {
            explain.logical_plan = planner::PlanFormatter::renderTree(planner::PlanFormatter::buildFormattedTree(logical_plan.get()), false);
        }
        
        optimizer_->clearAppliedRules();
        auto optimized_plan = optimizer_->optimize(std::move(logical_plan));
        if (is_explain) {
            explain.optimized_plan = planner::PlanFormatter::renderTree(planner::PlanFormatter::buildFormattedTree(optimized_plan.get()), false);
            explain.optimizer_rules = optimizer_->getAppliedRules();
        }
        
        auto physical_plan = physical_planner_->plan(std::move(optimized_plan));
        
        planner::FormattedPlanNode formatted_phys;
        if (is_explain) {
            std::function<void(planner::AbstractPlanNode*, planner::FormattedPlanNode&)> attachStats = [&](planner::AbstractPlanNode* node, planner::FormattedPlanNode& fmt_node) {
                auto stats = std::make_shared<executor::ExecutionStats>();
                node->setStats(stats);
                fmt_node.stats = stats;
                for (size_t i = 0; i < node->getChildren().size(); ++i) {
                    attachStats(node->getChildren()[i].get(), fmt_node.children[i]);
                }
            };
            formatted_phys = planner::PlanFormatter::buildFormattedTree(physical_plan.get());
            if (is_analyze) {
                attachStats(physical_plan.get(), formatted_phys);
            }
            explain.physical_plan = planner::PlanFormatter::renderTree(formatted_phys, false);
            
            std::ostringstream schema_out;
            const auto& schema = physical_plan->getOutputSchema();
            schema_out << "Schema(";
            for (size_t i = 0; i < schema.getColumnCount(); ++i) {
                schema_out << schema.getColumn(i).getName();
                if (i + 1 < schema.getColumnCount()) schema_out << ", ";
            }
            schema_out << ")";
            explain.output_schema = schema_out.str();
        }
        
        if (is_explain && !is_analyze) {
            out << "=== LOGICAL PLAN ===\n" << explain.logical_plan << "\n";
            out << "=== OPTIMIZED LOGICAL PLAN ===\n" << explain.optimized_plan << "\n";
            out << "=== PHYSICAL PLAN ===\n" << explain.physical_plan << "\n";
            out << "=== OUTPUT SCHEMA ===\n" << explain.output_schema << "\n\n";
            out << "=== OPTIMIZER RULES APPLIED ===\n";
            if (explain.optimizer_rules.empty()) {
                out << "(None)\n";
            } else {
                for (const auto& rule : explain.optimizer_rules) {
                    out << rule << "\n";
                }
            }
            return;
        }
        
        auto *txn = txn_manager_->begin();
        ExecutorContext exec_ctx(txn, catalog_.get(), bpm_.get(), mvcc_manager_.get(), disk_manager_.get(), lock_manager_.get(), log_manager_.get());
        
        auto exec = planner::ExecutorFactory::createExecutor(&exec_ctx, std::move(physical_plan));
        exec->init();
        
        const auto& schema = exec->outputSchema();
        TablePrinter printer;
        
        std::vector<std::string> columns;
        for (size_t i = 0; i < schema.getColumnCount(); ++i) {
            columns.push_back(schema.getColumn(i).getName());
        }
        printer.setSchema(columns);
        
        Tuple tuple;
        RID rid;
        size_t row_count = 0;
        
        while (exec->next(&tuple, &rid)) {
            if (!is_explain) {
                std::vector<std::string> row;
                for (size_t i = 0; i < schema.getColumnCount(); ++i) {
                    ColumnValueExpression col_expr(i);
                    Value val = col_expr.evaluate(tuple, schema);
                    if (val.isNull()) {
                        row.emplace_back("NULL");
                    } else if (val.getType() == TypeId::Integer) {
                        row.push_back(std::to_string(val.getAsInteger()));
                    } else if (val.getType() == TypeId::Boolean) {
                        row.emplace_back(val.getAsBoolean() ? "true" : "false");
                    } else if (val.getType() == TypeId::Varchar) {
                        row.push_back(val.getAsVarchar());
                    } else {
                        row.emplace_back("?");
                    }
                }
                printer.addRow(row);
            }
            row_count++;
        }
        
        if (!is_explain) {
            printer.print(out);
            TablePrinter::printRowCount(out, row_count);
        } else if (is_analyze) {
            planner::PlanFormatter::computeRowsIn(formatted_phys);
            explain.physical_plan = planner::PlanFormatter::renderTree(formatted_phys, true);
            out << "=== LOGICAL PLAN ===\n" << explain.logical_plan << "\n";
            out << "=== OPTIMIZED LOGICAL PLAN ===\n" << explain.optimized_plan << "\n";
            out << "=== PHYSICAL PLAN ===\n" << explain.physical_plan << "\n";
            out << "=== OUTPUT SCHEMA ===\n" << explain.output_schema << "\n\n";
            out << "=== OPTIMIZER RULES APPLIED ===\n";
            if (explain.optimizer_rules.empty()) {
                out << "(None)\n";
            } else {
                for (const auto& rule : explain.optimizer_rules) {
                    out << rule << "\n";
                }
            }
        }
        
        txn_manager_->commit(txn);
        
    } catch (const std::exception& e) {
        out << "Error: " << e.what() << "\n";
    }
}

} // namespace hamdb::shell
