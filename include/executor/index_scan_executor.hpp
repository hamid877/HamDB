#pragma once

#include "catalog/table_info.hpp"
#include "executor/abstract_executor.hpp"
#include "executor/executor_context.hpp"
#include "executor/expression.hpp"
#include "index/bplus_tree.hpp"
#include "storage/table_heap.hpp"

#include <optional>
#include <limits>

namespace hamdb
{

    class IndexScanExecutor : public AbstractExecutor
    {
    public:
        IndexScanExecutor(ExecutorContext* exec_ctx, const TableInfo* table_info,
                          std::unique_ptr<Expression> predicate, std::size_t limit = std::numeric_limits<std::size_t>::max(), std::size_t offset = 0);

        void init() override;
        bool next(Tuple* tuple, RID* rid) override;
        const Schema& outputSchema() const override;

    private:
        ExecutorContext* exec_ctx_;
        const TableInfo* table_info_;
        std::unique_ptr<Expression> predicate_;
        bool is_done_{false};
        std::optional<BPlusTree> bplus_tree_;
        std::optional<TableHeap> table_heap_;
        std::optional<BPlusTreeIterator> iter_;
        std::size_t limit_;
        std::size_t offset_;
        std::size_t tuples_emitted_{0};
        std::size_t tuples_skipped_{0};
    };

} // namespace hamdb
