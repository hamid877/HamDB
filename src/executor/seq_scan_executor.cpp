#include "executor/seq_scan_executor.hpp"

namespace hamdb
{

    SeqScanExecutor::SeqScanExecutor(ExecutorContext* exec_ctx, const TableInfo* table_info, std::unique_ptr<Expression> predicate, std::size_t limit, std::size_t offset)
        : exec_ctx_(exec_ctx), table_info_(table_info), predicate_(std::move(predicate)), limit_(limit), offset_(offset)
    {
    }

    void SeqScanExecutor::init()
    {
        auto heap_opt =
            TableHeap::open(*exec_ctx_->getDiskManager(), table_info_->getHeapRootPage());
        if (heap_opt)
        {
            table_heap_.emplace(std::move(*heap_opt));
            iter_.emplace(table_heap_->begin());
        }
        else
        {
            iter_ = std::nullopt;
            table_heap_ = std::nullopt;
        }
        tuples_emitted_ = 0;
        tuples_skipped_ = 0;
    }

    bool SeqScanExecutor::next(Tuple* tuple, RID* rid)
    {
        if (!table_heap_ || !iter_)
        {
            return false;
        }

        if (limit_ != std::numeric_limits<std::size_t>::max() && limit_ > 0 && tuples_emitted_ >= limit_)
        {
            return false;
        }
        if (limit_ == 0) {
            return false;
        }

        while (iter_.value() != table_heap_->end())
        {
            Tuple heap_tuple = *iter_.value();
            RID current_rid = iter_.value().getRID();
            ++iter_.value();

            Transaction* txn = exec_ctx_->getTransaction();
            MvccManager* mvcc = exec_ctx_->getMvccManager();

            if (mvcc->versionCount(current_rid) > 0)
            {
                auto visible_data = mvcc->read(txn, current_rid);
                if (visible_data.has_value())
                {
                    *tuple = Tuple(*visible_data);
                    *rid = current_rid;
                    
                    if (predicate_) {
                        auto val = predicate_->evaluate(*tuple, table_info_->getSchema());
                        if (val.isNull() || !val.getAsBoolean()) {
                            continue;
                        }
                    }
                    
                    if (tuples_skipped_ < offset_) {
                        tuples_skipped_++;
                        continue;
                    }
                    tuples_emitted_++;
                    return true;
                }
                // If std::nullopt, the tuple is invisible or deleted in MVCC, skip it.
            }
            else
            {
                // No MVCC version chain, meaning the heap tuple is the committed version.
                *tuple = std::move(heap_tuple);
                *rid = current_rid;
                
                if (predicate_) {
                    auto val = predicate_->evaluate(*tuple, table_info_->getSchema());
                    if (val.isNull() || !val.getAsBoolean()) {
                        continue;
                    }
                }
                
                if (tuples_skipped_ < offset_) {
                    tuples_skipped_++;
                    continue;
                }
                tuples_emitted_++;
                return true;
            }
        }
        return false;
    }

    const Schema& SeqScanExecutor::outputSchema() const
    {
        return table_info_->getSchema();
    }

} // namespace hamdb
