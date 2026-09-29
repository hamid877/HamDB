#include "shell/shell.hpp"
#include "shell/table_printer.hpp"
#include "parser/parser.hpp"
#include "binder/binder.hpp"
#include "planner/executor_factory.hpp"
#include "executor/column_value_expression.hpp"
#include "planner/explain_plan.hpp"
#include "planner/plan_formatter.hpp"
#include "planner/nested_loop_join_plan.hpp"
#include "planner/hash_join_plan.hpp"
#include "planner/aggregation_plan.hpp"
#include "planner/having_plan.hpp"
#include "planner/order_by_plan.hpp"
#include "shell/script_executor.hpp"
#include "shell/meta_commands.hpp"

#include <stdexcept>
#include <sstream>

namespace hamdb::shell {

namespace {
std::unique_ptr<planner::AbstractPlanNode> clonePhysicalPlan(const planner::AbstractPlanNode* plan) {
    if (!plan) return nullptr;
    std::unique_ptr<planner::AbstractPlanNode> cloned;
    switch (plan->getType()) {
        case planner::PhysicalPlanType::SEQ_SCAN: {
            auto* node = static_cast<const planner::SeqScanPlan*>(plan);
            cloned = std::make_unique<planner::SeqScanPlan>(
                node->getOutputSchema(), node->getTableName(), node->getTableAlias(),
                node->getPredicate() ? node->getPredicate()->clone() : nullptr,
                node->getLimit(), node->getOffset());
            break;
        }
        case planner::PhysicalPlanType::FILTER: {
            auto* node = static_cast<const planner::FilterPlan*>(plan);
            cloned = std::make_unique<planner::FilterPlan>(
                node->getOutputSchema(), node->getPredicate() ? node->getPredicate()->clone() : nullptr);
            break;
        }
        case planner::PhysicalPlanType::PROJECTION: {
            auto* node = static_cast<const planner::ProjectionPlan*>(plan);
            std::vector<std::unique_ptr<Expression>> exprs;
            for (const auto& e : node->getExpressions()) exprs.push_back(e ? e->clone() : nullptr);
            cloned = std::make_unique<planner::ProjectionPlan>(node->getOutputSchema(), std::move(exprs));
            break;
        }
        case planner::PhysicalPlanType::SORT: {
            auto* node = static_cast<const planner::SortPlan*>(plan);
            std::vector<std::pair<OrderByType, std::unique_ptr<Expression>>> order_by;
            for (const auto& p : node->getOrderBy()) order_by.emplace_back(p.first, p.second ? p.second->clone() : nullptr);
            cloned = std::make_unique<planner::SortPlan>(node->getOutputSchema(), std::move(order_by));
            break;
        }
        case planner::PhysicalPlanType::ORDER_BY: {
            auto* node = static_cast<const planner::OrderByPlan*>(plan);
            std::vector<std::pair<OrderByDirection, std::unique_ptr<Expression>>> order_by;
            for (const auto& p : node->getOrderBy()) order_by.emplace_back(p.first, p.second ? p.second->clone() : nullptr);
            cloned = std::make_unique<planner::OrderByPlan>(node->getOutputSchema(), std::move(order_by));
            break;
        }
        case planner::PhysicalPlanType::LIMIT: {
            auto* node = static_cast<const planner::LimitPlan*>(plan);
            cloned = std::make_unique<planner::LimitPlan>(node->getOutputSchema(), node->getLimit(), node->getOffset());
            break;
        }
        case planner::PhysicalPlanType::VALUES: {
            auto* node = static_cast<const planner::ValuesPlan*>(plan);
            std::vector<std::vector<std::unique_ptr<Expression>>> values;
            for (const auto& row : node->getValues()) {
                std::vector<std::unique_ptr<Expression>> r;
                for (const auto& e : row) r.push_back(e ? e->clone() : nullptr);
                values.push_back(std::move(r));
            }
            cloned = std::make_unique<planner::ValuesPlan>(node->getOutputSchema(), std::move(values));
            break;
        }
        case planner::PhysicalPlanType::INSERT: {
            auto* node = static_cast<const planner::InsertPlan*>(plan);
            cloned = std::make_unique<planner::InsertPlan>(node->getOutputSchema(), node->getTableName());
            break;
        }
        case planner::PhysicalPlanType::UPDATE: {
            auto* node = static_cast<const planner::UpdatePlan*>(plan);
            std::vector<std::unique_ptr<Expression>> exprs;
            for (const auto& e : node->getTargetExpressions()) exprs.push_back(e ? e->clone() : nullptr);
            cloned = std::make_unique<planner::UpdatePlan>(node->getOutputSchema(), node->getTableName(), std::move(exprs));
            break;
        }
        case planner::PhysicalPlanType::DELETE: {
            auto* node = static_cast<const planner::DeletePlan*>(plan);
            cloned = std::make_unique<planner::DeletePlan>(node->getOutputSchema(), node->getTableName());
            break;
        }
                case planner::PhysicalPlanType::NESTED_LOOP_JOIN: {
            auto* node = static_cast<const planner::NestedLoopJoinPlan*>(plan);
            cloned = std::make_unique<planner::NestedLoopJoinPlan>(
                node->getOutputSchema(),
                node->getPredicate() ? node->getPredicate()->clone() : nullptr);
            break;
        }
        case planner::PhysicalPlanType::HASH_JOIN: {
            auto* node = static_cast<const planner::HashJoinPlan*>(plan);
            cloned = std::make_unique<planner::HashJoinPlan>(
                node->getOutputSchema(),
                node->getLeftKeyExpr() ? node->getLeftKeyExpr()->clone() : nullptr,
                node->getRightKeyExpr() ? node->getRightKeyExpr()->clone() : nullptr);
            break;
        }
        case planner::PhysicalPlanType::INDEX_SCAN: {
            auto* node = static_cast<const planner::IndexScanPlan*>(plan);
            cloned = std::make_unique<planner::IndexScanPlan>(
                node->getOutputSchema(), node->getTableName(), node->getTableAlias(),
                node->getPredicate() ? node->getPredicate()->clone() : nullptr,
                node->getLimit(), node->getOffset());
            break;
        }
        case planner::PhysicalPlanType::AGGREGATION: {
            auto* node = static_cast<const planner::AggregationPlan*>(plan);
            std::vector<std::unique_ptr<hamdb::Expression>> group_bys;
            for (const auto& expr : node->getGroupBys()) {
                group_bys.push_back(expr->clone());
            }
            std::vector<std::unique_ptr<hamdb::Expression>> aggregates;
            for (const auto& expr : node->getAggregates()) {
                aggregates.push_back(expr ? expr->clone() : nullptr);
            }
            cloned = std::make_unique<planner::AggregationPlan>(node->getOutputSchema(), std::move(group_bys), std::move(aggregates), node->getAggTypes());
            break;
        }
        case planner::PhysicalPlanType::HAVING: {
            auto* node = static_cast<const planner::HavingPlan*>(plan);
            cloned = std::make_unique<planner::HavingPlan>(
                node->getOutputSchema(),
                node->getPredicate() ? node->getPredicate()->clone() : nullptr);
            break;
        }
    }
    for (const auto& child : plan->getChildren()) {
        cloned->addChild(clonePhysicalPlan(child.get()));
    }
    if (plan->getStats()) cloned->setStats(plan->getStats());
    return cloned;
}

void bindPhysicalPlan(planner::AbstractPlanNode* plan, const std::vector<Value>& params) {
    if (!plan) return;
    switch (plan->getType()) {
        case planner::PhysicalPlanType::SEQ_SCAN: {
            auto* node = static_cast<planner::SeqScanPlan*>(plan);
            if (node->getPredicate()) node->getPredicate()->bindParameters(params);
            break;
        }
        case planner::PhysicalPlanType::FILTER: {
            auto* node = static_cast<planner::FilterPlan*>(plan);
            if (node->getPredicate()) node->getPredicate()->bindParameters(params);
            break;
        }
        case planner::PhysicalPlanType::PROJECTION: {
            auto* node = static_cast<planner::ProjectionPlan*>(plan);
            for (auto& e : node->getExpressions()) if (e) e.get()->bindParameters(params);
            break;
        }
        case planner::PhysicalPlanType::SORT: {
            auto* node = static_cast<planner::SortPlan*>(plan);
            for (auto& p : node->getOrderBy()) if (p.second) p.second.get()->bindParameters(params);
            break;
        }
        case planner::PhysicalPlanType::ORDER_BY: {
            auto* node = static_cast<planner::OrderByPlan*>(plan);
            for (auto& p : node->getOrderBy()) if (p.second) p.second.get()->bindParameters(params);
            break;
        }
        case planner::PhysicalPlanType::VALUES: {
            auto* node = static_cast<planner::ValuesPlan*>(plan);
            for (auto& row : node->getValues()) {
                for (auto& e : row) if (e) e.get()->bindParameters(params);
            }
            break;
        }
        case planner::PhysicalPlanType::UPDATE: {
            auto* node = static_cast<planner::UpdatePlan*>(plan);
            for (auto& e : node->getTargetExpressions()) if (e) e.get()->bindParameters(params);
            break;
        }
                case planner::PhysicalPlanType::NESTED_LOOP_JOIN: {
            auto* node = static_cast<planner::NestedLoopJoinPlan*>(plan);
            if (node->getPredicate()) node->getPredicate()->bindParameters(params);
            break;
        }
        case planner::PhysicalPlanType::HASH_JOIN: {
            auto* node = static_cast<planner::HashJoinPlan*>(plan);
            if (node->getMutableLeftKeyExpr()) node->getMutableLeftKeyExpr()->bindParameters(params);
            if (node->getMutableRightKeyExpr()) node->getMutableRightKeyExpr()->bindParameters(params);
            break;
        }
        case planner::PhysicalPlanType::INDEX_SCAN: {
            auto* node = static_cast<planner::IndexScanPlan*>(plan);
            if (node->getPredicate()) node->getPredicate()->bindParameters(params);
            break;
        }
        case planner::PhysicalPlanType::HAVING: {
            auto* node = static_cast<planner::HavingPlan*>(plan);
            if (node->getPredicate()) node->getPredicate()->bindParameters(params);
            break;
        }
        default: break;
    }
    for (auto& child : plan->getChildren()) {
        bindPhysicalPlan(child.get(), params);
    }
}
} // namespace

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
    
    optimizer_ = std::make_unique<optimizer::HamDBOptimizer>(catalog_.get());
    prep_manager_ = std::make_unique<PreparedStatementManager>();
}

void Shell::executeMeta(const std::string& cmd, std::ostream& out) {
    MetaCommands::execute(cmd, *this, out);
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
        
        if (auto* dealloc_stmt = dynamic_cast<ast::DeallocateStatement*>(inner_stmt)) {
            prep_manager_->removeStatement(dealloc_stmt->name);
            out << "Statement deallocated.\n";
            return;
        }
        
        if (auto* prep_stmt = dynamic_cast<ast::PrepareStatement*>(inner_stmt)) {
            binder::Binder binder(catalog_.get());
            auto bound_stmt = binder.bind(*prep_stmt->query);
            auto param_types = binder.getParameterTypes();
            
            auto logical_plan = planner_->plan(std::move(bound_stmt));
            optimizer_->clearAppliedRules();
            auto optimized_plan = optimizer_->optimize(std::move(logical_plan));
            auto physical_plan = physical_planner_->plan(std::move(optimized_plan));
            
            auto prep = std::make_unique<PreparedStatement>(
                prep_stmt->name, nullptr, std::move(physical_plan), param_types);
            prep_manager_->addStatement(std::move(prep));
            out << "Statement prepared.\n";
            return;
        }

        planner::ExplainPlan explain;
        std::unique_ptr<planner::AbstractPlanNode> physical_plan;
        
        if (auto* exec_stmt = dynamic_cast<ast::ExecuteStatement*>(inner_stmt)) {
            PreparedStatement* prepared = prep_manager_->getStatement(exec_stmt->name);
            if (!prepared) {
                out << "Error: Prepared statement not found: " << exec_stmt->name << "\n";
                return;
            }
            if (prepared->getParameterTypes().size() != exec_stmt->parameters.size()) {
                out << "Error: Parameter count mismatch for " << exec_stmt->name << "\n";
                return;
            }
            
            std::vector<Value> params;
            for (size_t i = 0; i < exec_stmt->parameters.size(); ++i) {
                binder::Binder binder(catalog_.get());
                auto bound_expr = binder.bindExpression(*exec_stmt->parameters[i]);
                auto exec_expr = bound_expr->takeExpr();
                Value val = exec_expr->evaluate(Tuple{}, Schema(std::vector<Column>{}));
                TypeId expected = prepared->getParameterTypes()[i];
                if (val.getType() != expected && val.getType() != TypeId::Null) {
                    out << "Error: Type mismatch for parameter\n";
                    return;
                }
                params.push_back(val);
            }
            
            physical_plan = clonePhysicalPlan(prepared->getPhysicalPlan());
            bindPhysicalPlan(physical_plan.get(), params);
            
            if (is_explain) {
                // EXPLAIN EXECUTE is not supported in details, just show physical plan
                explain.physical_plan = planner::PlanFormatter::renderTree(planner::PlanFormatter::buildFormattedTree(physical_plan.get()), false);
                explain.logical_plan = "(Optimized Logical Plan from cache)";
                explain.optimized_plan = "(Optimized Logical Plan from cache)";
                
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
        } else {
            // Normal execution
            binder::Binder binder(catalog_.get());
            auto bound_stmt = binder.bind(*inner_stmt);
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
            
            physical_plan = physical_planner_->plan(std::move(optimized_plan));
        }

        
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
