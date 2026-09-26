#pragma once

#include "catalog/table_info.hpp"
#include "executor/abstract_executor.hpp"
#include "executor/executor_context.hpp"
#include "storage/heap_iterator.hpp"
#include "storage/table_heap.hpp"
#include "executor/expression.hpp"

#include <memory>
#include <optional>

namespace hamdb
{

    class SeqScanExecutor : public AbstractExecutor
    {
    public:
        SeqScanExecutor(ExecutorContext* exec_ctx, const TableInfo* table_info, std::unique_ptr<Expression> predicate = nullptr);

        void init() override;
        bool next(Tuple* tuple, RID* rid) override;
        const Schema& outputSchema() const override;

    private:
        ExecutorContext* exec_ctx_;
        const TableInfo* table_info_;
        std::unique_ptr<Expression> predicate_;
        std::optional<TableHeap> table_heap_;
        std::optional<HeapIterator> iter_;
    };

} // namespace hamdb
