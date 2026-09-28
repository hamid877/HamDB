#include "planner/executor_factory.hpp"
#include "executor/nested_loop_join_executor.hpp"
#include "executor/hash_join_executor.hpp"
#include "planner/nested_loop_join_plan.hpp"
#include "planner/hash_join_plan.hpp"
#include "planner/aggregation_plan.hpp"
#include "executor/seq_scan_executor.hpp"
#include "executor/index_scan_executor.hpp"
#include "executor/filter_executor.hpp"
#include "executor/projection_executor.hpp"
#include "executor/sort_executor.hpp"
#include "executor/limit_executor.hpp"
#include "executor/values_executor.hpp"
#include "executor/insert_executor.hpp"
#include "executor/update_executor.hpp"
#include "executor/delete_executor.hpp"
#include "executor/aggregation_executor.hpp"
#include <stdexcept>
#include <chrono>

namespace hamdb::planner {

namespace {
class AnalyzeExecutor : public hamdb::AbstractExecutor {
public:
    AnalyzeExecutor(std::unique_ptr<hamdb::AbstractExecutor> child, std::shared_ptr<hamdb::executor::ExecutionStats> stats)
        : child_(std::move(child)), stats_(std::move(stats)) {}
    
    void init() override {
        auto start = std::chrono::steady_clock::now();
        child_->init();
        auto end = std::chrono::steady_clock::now();
        stats_->execution_time += std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    }
    
    bool next(hamdb::Tuple* tuple, hamdb::RID* rid) override {
        auto start = std::chrono::steady_clock::now();
        bool res = child_->next(tuple, rid);
        auto end = std::chrono::steady_clock::now();
        stats_->execution_time += std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        if (res) {
            stats_->rows_out++;
        }
        return res;
    }
    
    const hamdb::Schema& outputSchema() const override {
        return child_->outputSchema();
    }
private:
    std::unique_ptr<hamdb::AbstractExecutor> child_;
    std::shared_ptr<hamdb::executor::ExecutionStats> stats_;
};
} // namespace

std::unique_ptr<hamdb::AbstractExecutor> ExecutorFactory::createExecutor(
    hamdb::ExecutorContext* exec_ctx,
    std::unique_ptr<AbstractPlanNode> plan) {
    
    if (!plan) return nullptr;
    
    std::vector<std::unique_ptr<hamdb::AbstractExecutor>> child_executors;
    for (auto& child_plan : plan->getChildren()) {
        child_executors.push_back(createExecutor(exec_ctx, std::move(child_plan)));
    }

    std::unique_ptr<hamdb::AbstractExecutor> exec;
    switch (plan->getType()) {
        case PhysicalPlanType::SEQ_SCAN: {
            auto* seq_scan = dynamic_cast<SeqScanPlan*>(plan.get());
            TableInfo* table_info = nullptr;
            if (exec_ctx->getCatalog()->getTable(seq_scan->getTableName(), table_info) != Status::Ok) {
                throw std::runtime_error("Table not found: " + seq_scan->getTableName());
            }
            exec = std::make_unique<SeqScanExecutor>(exec_ctx, table_info, std::move(seq_scan->getPredicate()), seq_scan->getLimit(), seq_scan->getOffset());
            break;
        }
        case PhysicalPlanType::FILTER: {
            auto* filter = dynamic_cast<FilterPlan*>(plan.get());
            exec = std::make_unique<FilterExecutor>(
                std::move(child_executors[0]), std::move(filter->getPredicate()));
            break;
        }
        case PhysicalPlanType::PROJECTION: {
            auto* proj = dynamic_cast<ProjectionPlan*>(plan.get());
            exec = std::make_unique<ProjectionExecutor>(
                std::move(child_executors[0]), std::move(proj->getExpressions()), proj->getOutputSchema());
            break;
        }
        case PhysicalPlanType::SORT: {
            auto* sort = dynamic_cast<SortPlan*>(plan.get());
            exec = std::make_unique<SortExecutor>(
                std::move(child_executors[0]), std::move(sort->getOrderBy()));
            break;
        }
        case PhysicalPlanType::LIMIT: {
            auto* limit = dynamic_cast<LimitPlan*>(plan.get());
            exec = std::make_unique<LimitExecutor>(
                std::move(child_executors[0]), limit->getLimit(), limit->getOffset());
            break;
        }
        case PhysicalPlanType::VALUES: {
            auto* values = dynamic_cast<ValuesPlan*>(plan.get());
            exec = std::make_unique<ValuesExecutor>(
                std::move(values->getValues()), values->getOutputSchema());
            break;
        }
        case PhysicalPlanType::INSERT: {
            auto* insert = dynamic_cast<InsertPlan*>(plan.get());
            TableInfo* table_info = nullptr;
            if (exec_ctx->getCatalog()->getTable(insert->getTableName(), table_info) != Status::Ok) {
                throw std::runtime_error("Table not found: " + insert->getTableName());
            }
            exec = std::make_unique<InsertExecutor>(
                exec_ctx, table_info, std::move(child_executors[0]));
            break;
        }
        case PhysicalPlanType::UPDATE: {
            auto* update = dynamic_cast<UpdatePlan*>(plan.get());
            TableInfo* table_info = nullptr;
            if (exec_ctx->getCatalog()->getTable(update->getTableName(), table_info) != Status::Ok) {
                throw std::runtime_error("Table not found: " + update->getTableName());
            }
            exec = std::make_unique<UpdateExecutor>(
                exec_ctx, table_info, std::move(child_executors[0]), std::move(update->getTargetExpressions()));
            break;
        }
        case PhysicalPlanType::DELETE: {
            auto* del = dynamic_cast<DeletePlan*>(plan.get());
            TableInfo* table_info = nullptr;
            if (exec_ctx->getCatalog()->getTable(del->getTableName(), table_info) != Status::Ok) {
                throw std::runtime_error("Table not found: " + del->getTableName());
            }
            exec = std::make_unique<DeleteExecutor>(
                exec_ctx, table_info, std::move(child_executors[0]));
            break;
        }
        case PhysicalPlanType::INDEX_SCAN: {
            auto* index_scan = dynamic_cast<IndexScanPlan*>(plan.get());
            TableInfo* table_info = nullptr;
            if (exec_ctx->getCatalog()->getTable(index_scan->getTableName(), table_info) != Status::Ok) {
                throw std::runtime_error("Table not found: " + index_scan->getTableName());
            }
            exec = std::make_unique<IndexScanExecutor>(exec_ctx, table_info, std::move(index_scan->getPredicate()), index_scan->getLimit(), index_scan->getOffset());
            break;
        }
        
        case PhysicalPlanType::NESTED_LOOP_JOIN: {
            auto* join_plan = dynamic_cast<const planner::NestedLoopJoinPlan*>(plan.get());
            exec = std::make_unique<NestedLoopJoinExecutor>(
                std::move(child_executors[0]), std::move(child_executors[1]), const_cast<planner::NestedLoopJoinPlan*>(join_plan)->getPredicate() ? const_cast<planner::NestedLoopJoinPlan*>(join_plan)->getPredicate()->clone() : nullptr);
            break;
        }
        case PhysicalPlanType::HASH_JOIN: {
            auto* join_plan = dynamic_cast<const planner::HashJoinPlan*>(plan.get());
            exec = std::make_unique<HashJoinExecutor>(
                std::move(child_executors[0]), std::move(child_executors[1]), 
                const_cast<planner::HashJoinPlan*>(join_plan)->getLeftKeyExpr() ? const_cast<planner::HashJoinPlan*>(join_plan)->getLeftKeyExpr()->clone() : nullptr,
                const_cast<planner::HashJoinPlan*>(join_plan)->getRightKeyExpr() ? const_cast<planner::HashJoinPlan*>(join_plan)->getRightKeyExpr()->clone() : nullptr);
            break;
        }
        case PhysicalPlanType::AGGREGATION: {
            auto* agg_plan = dynamic_cast<const planner::AggregationPlan*>(plan.get());
            std::vector<std::unique_ptr<hamdb::Expression>> group_bys;
            std::vector<std::unique_ptr<hamdb::Expression>> aggregates;
            for (const auto& gb : agg_plan->getGroupBys()) {
                group_bys.push_back(gb->clone());
            }
            for (const auto& agg : agg_plan->getAggregates()) {
                aggregates.push_back(agg ? agg->clone() : nullptr);
            }
            exec = std::make_unique<AggregationExecutor>(
                std::move(child_executors[0]), std::move(group_bys), std::move(aggregates), agg_plan->getAggTypes(), agg_plan->getOutputSchema());
            break;
        }
        default:
            throw std::runtime_error("Unsupported physical plan type");
    }

    if (plan->getStats()) {
        return std::make_unique<AnalyzeExecutor>(std::move(exec), plan->getStats());
    }
    return exec;
}

} // namespace hamdb::planner
