#pragma once

#include "catalog/table_info.hpp"
#include "executor/abstract_executor.hpp"
#include "executor/executor_context.hpp"
#include "executor/expression.hpp"
#include "index/bplus_tree.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace hamdb
{

    class UpdateExecutor : public AbstractExecutor
    {
    public:
        UpdateExecutor(ExecutorContext* exec_ctx, const TableInfo* table_info,
                       std::unique_ptr<AbstractExecutor> child_executor,
                       std::vector<std::unique_ptr<Expression>> target_expressions);

        void init() override;
        bool next(Tuple* tuple, RID* rid) override;
        const Schema& outputSchema() const override;

    private:
        ExecutorContext* exec_ctx_;
        const TableInfo* table_info_;
        std::unique_ptr<AbstractExecutor> child_executor_;
        std::vector<std::unique_ptr<Expression>> target_expressions_;
        std::optional<BPlusTree> bplus_tree_;
        bool is_done_{false};
        Schema output_schema_;
    };

} // namespace hamdb
