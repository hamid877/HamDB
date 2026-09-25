#pragma once

#include "executor/abstract_executor.hpp"
#include "executor/executor_context.hpp"
#include "catalog/table_info.hpp"
#include "storage/table_heap.hpp"
#include "index/bplus_tree.hpp"

#include <memory>
#include <optional>

namespace hamdb {

class InsertExecutor : public AbstractExecutor {
public:
    InsertExecutor(ExecutorContext* exec_ctx, const TableInfo* table_info, std::unique_ptr<AbstractExecutor> child_executor);

    void init() override;
    bool next(Tuple* tuple, RID* rid) override;
    const Schema& outputSchema() const override;

private:
    ExecutorContext* exec_ctx_;
    const TableInfo* table_info_;
    std::unique_ptr<AbstractExecutor> child_executor_;
    std::optional<TableHeap> table_heap_;
    std::optional<BPlusTree> bplus_tree_;
    bool is_done_{false};
    Schema output_schema_;
};

} // namespace hamdb
