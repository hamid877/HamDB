#include "planner/executor_factory.hpp"
#include "executor/seq_scan_executor.hpp"
#include "executor/filter_executor.hpp"
#include "executor/projection_executor.hpp"
#include "executor/sort_executor.hpp"
#include "executor/limit_executor.hpp"
#include "executor/values_executor.hpp"
#include "executor/insert_executor.hpp"
#include "executor/update_executor.hpp"
#include "executor/delete_executor.hpp"
#include <stdexcept>

namespace hamdb::planner {

std::unique_ptr<hamdb::AbstractExecutor> ExecutorFactory::createExecutor(
    hamdb::ExecutorContext* exec_ctx,
    std::unique_ptr<AbstractPlanNode> plan) {
    
    if (!plan) return nullptr;
    
    std::vector<std::unique_ptr<hamdb::AbstractExecutor>> child_executors;
    for (auto& child_plan : plan->getChildren()) {
        child_executors.push_back(createExecutor(exec_ctx, std::move(child_plan)));
    }

    switch (plan->getType()) {
        case PhysicalPlanType::SEQ_SCAN: {
            auto* seq_scan = dynamic_cast<SeqScanPlan*>(plan.get());
            TableInfo* table_info = nullptr;
            if (exec_ctx->getCatalog()->getTable(seq_scan->getTableName(), table_info) != Status::Ok) {
                throw std::runtime_error("Table not found: " + seq_scan->getTableName());
            }
            return std::make_unique<SeqScanExecutor>(exec_ctx, table_info, std::move(seq_scan->getPredicate()));
        }
        case PhysicalPlanType::FILTER: {
            auto* filter = dynamic_cast<FilterPlan*>(plan.get());
            return std::make_unique<FilterExecutor>(
                std::move(child_executors[0]), std::move(filter->getPredicate()));
        }
        case PhysicalPlanType::PROJECTION: {
            auto* proj = dynamic_cast<ProjectionPlan*>(plan.get());
            return std::make_unique<ProjectionExecutor>(
                std::move(child_executors[0]), std::move(proj->getExpressions()), proj->getOutputSchema());
        }
        case PhysicalPlanType::SORT: {
            auto* sort = dynamic_cast<SortPlan*>(plan.get());
            return std::make_unique<SortExecutor>(
                std::move(child_executors[0]), std::move(sort->getOrderBy()));
        }
        case PhysicalPlanType::LIMIT: {
            auto* limit = dynamic_cast<LimitPlan*>(plan.get());
            return std::make_unique<LimitExecutor>(
                std::move(child_executors[0]), limit->getLimit(), limit->getOffset());
        }
        case PhysicalPlanType::VALUES: {
            auto* values = dynamic_cast<ValuesPlan*>(plan.get());
            return std::make_unique<ValuesExecutor>(
                std::move(values->getValues()), values->getOutputSchema());
        }
        case PhysicalPlanType::INSERT: {
            auto* insert = dynamic_cast<InsertPlan*>(plan.get());
            TableInfo* table_info = nullptr;
            if (exec_ctx->getCatalog()->getTable(insert->getTableName(), table_info) != Status::Ok) {
                throw std::runtime_error("Table not found: " + insert->getTableName());
            }
            return std::make_unique<InsertExecutor>(
                exec_ctx, table_info, std::move(child_executors[0]));
        }
        case PhysicalPlanType::UPDATE: {
            auto* update = dynamic_cast<UpdatePlan*>(plan.get());
            TableInfo* table_info = nullptr;
            if (exec_ctx->getCatalog()->getTable(update->getTableName(), table_info) != Status::Ok) {
                throw std::runtime_error("Table not found: " + update->getTableName());
            }
            return std::make_unique<UpdateExecutor>(
                exec_ctx, table_info, std::move(child_executors[0]), std::move(update->getTargetExpressions()));
        }
        case PhysicalPlanType::DELETE: {
            auto* del = dynamic_cast<DeletePlan*>(plan.get());
            TableInfo* table_info = nullptr;
            if (exec_ctx->getCatalog()->getTable(del->getTableName(), table_info) != Status::Ok) {
                throw std::runtime_error("Table not found: " + del->getTableName());
            }
            return std::make_unique<DeleteExecutor>(
                exec_ctx, table_info, std::move(child_executors[0]));
        }
        default:
            throw std::runtime_error("Unsupported physical plan type");
    }
}

} // namespace hamdb::planner
