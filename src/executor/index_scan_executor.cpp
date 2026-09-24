#include "executor/index_scan_executor.hpp"

namespace hamdb {

IndexScanExecutor::IndexScanExecutor(ExecutorContext* exec_ctx, const TableInfo* table_info, int64_t search_key)
    : exec_ctx_(exec_ctx), table_info_(table_info), search_key_(search_key), is_done_(false) {}

void IndexScanExecutor::init() {
    is_done_ = false;

    if (table_info_->getIndexRootPage() != kInvalidPageId) {
        bplus_tree_.emplace(*exec_ctx_->getBufferPoolManager());
        bplus_tree_->open(table_info_->getIndexRootPage());
    } else {
        bplus_tree_ = std::nullopt;
    }

    auto heap_opt = TableHeap::open(*exec_ctx_->getDiskManager(), table_info_->getHeapRootPage());
    if (heap_opt) {
        table_heap_.emplace(std::move(*heap_opt));
    } else {
        table_heap_ = std::nullopt;
    }
}

bool IndexScanExecutor::next(Tuple* tuple, RID* rid) {
    if (is_done_ || !bplus_tree_ || !table_heap_) {
        return false;
    }
    is_done_ = true;

    auto rid_opt = bplus_tree_->getValue(search_key_);
    if (!rid_opt) {
        return false;
    }

    RID current_rid = *rid_opt;
    Transaction* txn = exec_ctx_->getTransaction();
    MvccManager* mvcc = exec_ctx_->getMvccManager();

    if (mvcc->versionCount(current_rid) > 0) {
        auto visible_data = mvcc->read(txn, current_rid);
        if (visible_data.has_value()) {
            *tuple = Tuple(*visible_data);
            *rid = current_rid;
            return true;
        }
        return false;
    } else {
        Status s = table_heap_->readTuple(current_rid, *tuple);
        if (s == Status::Ok) {
            *rid = current_rid;
            return true;
        }
        return false;
    }
}

const Schema& IndexScanExecutor::outputSchema() const {
    return table_info_->getSchema();
}

} // namespace hamdb
