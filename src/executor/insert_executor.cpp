#include "executor/insert_executor.hpp"
#include "executor/column_value_expression.hpp"
#include "utils/serializer.hpp"
#include "wal/log_record.hpp"

namespace hamdb
{

    InsertExecutor::InsertExecutor(ExecutorContext* exec_ctx, const TableInfo* table_info,
                                   std::unique_ptr<AbstractExecutor> child_executor)
        : exec_ctx_(exec_ctx), table_info_(table_info), child_executor_(std::move(child_executor))
    {
        std::vector<Column> cols;
        cols.emplace_back("count", ColumnType::Integer);
        output_schema_ = Schema(std::move(cols));
    }

    void InsertExecutor::init()
    {
        is_done_ = false;
        child_executor_->init();

        auto heap_opt =
            TableHeap::open(*exec_ctx_->getDiskManager(), table_info_->getHeapRootPage());
        if (heap_opt)
        {
            table_heap_.emplace(std::move(*heap_opt));
        }
        else
        {
            table_heap_ = std::nullopt;
        }

        if (table_info_->getIndexRootPage() != kInvalidPageId)
        {
            bplus_tree_.emplace(*exec_ctx_->getBufferPoolManager());
            bplus_tree_->open(table_info_->getIndexRootPage());
        }
        else
        {
            bplus_tree_ = std::nullopt;
        }
    }

    bool InsertExecutor::next(Tuple* tuple, RID* rid)
    {
        if (is_done_ || !table_heap_)
        {
            return false;
        }

        int32_t count = 0;
        Tuple child_tuple;
        RID child_rid;

        Transaction* txn = exec_ctx_->getTransaction();
        MvccManager* mvcc = exec_ctx_->getMvccManager();
        LockManager* lock_mgr = exec_ctx_->getLockManager();
        LogManager* log_mgr = exec_ctx_->getLogManager();

        while (child_executor_->next(&child_tuple, &child_rid))
        {
            RID new_rid;
            Status status = table_heap_->insertTuple(child_tuple, new_rid);
            if (status != Status::Ok)
            {
                continue;
            }

            // Acquire exclusive lock
            lock_mgr->lockExclusive(txn, new_rid);

            // Create MVCC version
            std::vector<std::byte> data(child_tuple.data().begin(), child_tuple.data().end());
            if (!mvcc->insert(txn, new_rid, std::move(data)))
            {
                continue;
            }

            // Update primary B+ Tree index
            if (bplus_tree_)
            {
                int64_t key = ColumnValueExpression(0)
                                  .evaluate(child_tuple, table_info_->getSchema())
                                  .getAsInteger();
                (void)bplus_tree_->insert(key, new_rid);
            }

            // Append WAL INSERT record
            LogRecord log(txn->getTransactionId(), LogRecordType::INSERT, kInvalidLSN, new_rid,
                          child_tuple);
            log_mgr->append(log);

            count++;
        }

        std::vector<std::byte> buf(8);
        Serializer ser(buf);
        (void)ser.writeInt32(count);
        *tuple = Tuple(std::span<const std::byte>(buf.data(), ser.position()));
        *rid = RID(); // invalid rid

        is_done_ = true;
        return true;
    }

    const Schema& InsertExecutor::outputSchema() const
    {
        return output_schema_;
    }

} // namespace hamdb
