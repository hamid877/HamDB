#pragma once

#include "catalog/table_info.hpp"
#include "executor/abstract_executor.hpp"
#include "executor/executor_context.hpp"
#include "index/bplus_tree.hpp"
#include "storage/table_heap.hpp"

#include <optional>

namespace hamdb
{

    class IndexScanExecutor : public AbstractExecutor
    {
    public:
        IndexScanExecutor(ExecutorContext* exec_ctx, const TableInfo* table_info,
                          int64_t search_key);

        void init() override;
        bool next(Tuple* tuple, RID* rid) override;
        const Schema& outputSchema() const override;

    private:
        ExecutorContext* exec_ctx_;
        const TableInfo* table_info_;
        int64_t search_key_;
        bool is_done_{false};
        std::optional<BPlusTree> bplus_tree_;
        std::optional<TableHeap> table_heap_;
    };

} // namespace hamdb
