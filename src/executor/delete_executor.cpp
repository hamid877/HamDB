#include "executor/delete_executor.hpp"
#include "executor/column_value_expression.hpp"
#include "wal/log_record.hpp"
#include "utils/serializer.hpp"

namespace hamdb {

DeleteExecutor::DeleteExecutor(ExecutorContext* exec_ctx, const TableInfo* table_info, std::unique_ptr<AbstractExecutor> child_executor)
    : exec_ctx_(exec_ctx), table_info_(table_info), child_executor_(std::move(child_executor)) {
    std::vector<Column> cols;
    cols.emplace_back("count", ColumnType::Integer);
    output_schema_ = Schema(std::move(cols));
}

void DeleteExecutor::init() {
    is_done_ = false;
    child_executor_->init();

    if (table_info_->getIndexRootPage() != kInvalidPageId) {
        bplus_tree_.emplace(*exec_ctx_->getBufferPoolManager());
        bplus_tree_->open(table_info_->getIndexRootPage());
    } else {
        bplus_tree_ = std::nullopt;
    }
}

bool DeleteExecutor::next(Tuple* tuple, RID* rid) {
    if (is_done_) {
        return false;
    }
    
    int32_t count = 0;
    Tuple child_tuple;
    RID child_rid;

    Transaction* txn = exec_ctx_->getTransaction();
    MvccManager* mvcc = exec_ctx_->getMvccManager();
    LockManager* lock_mgr = exec_ctx_->getLockManager();
    LogManager* log_mgr = exec_ctx_->getLogManager();

    while (child_executor_->next(&child_tuple, &child_rid)) {
        // Acquire exclusive lock
        lock_mgr->lockExclusive(txn, child_rid);

        // Tombstone tuple via MVCC
        if (!mvcc->remove(txn, child_rid)) {
            continue;
        }

        // Remove index entry
        if (bplus_tree_) {
            int64_t key = ColumnValueExpression(0).evaluate(child_tuple, table_info_->getSchema()).getAsInteger();
            (void)bplus_tree_->remove(key);
        }

        // Append WAL DELETE record
        LogRecord log(txn->getTransactionId(), LogRecordType::DELETE, kInvalidLSN, child_rid, child_tuple);
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

const Schema& DeleteExecutor::outputSchema() const {
    return output_schema_;
}

} // namespace hamdb
