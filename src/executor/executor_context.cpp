#include "executor/executor_context.hpp"

namespace hamdb {

ExecutorContext::ExecutorContext(Transaction* txn, CatalogManager* catalog, BufferPoolManager* bpm, MvccManager* mvcc, DiskManager* disk_manager)
    : txn_(txn), catalog_(catalog), bpm_(bpm), mvcc_(mvcc), disk_manager_(disk_manager) {}

Transaction* ExecutorContext::getTransaction() const noexcept { return txn_; }
CatalogManager* ExecutorContext::getCatalog() const noexcept { return catalog_; }
BufferPoolManager* ExecutorContext::getBufferPoolManager() const noexcept { return bpm_; }
MvccManager* ExecutorContext::getMvccManager() const noexcept { return mvcc_; }
DiskManager* ExecutorContext::getDiskManager() const noexcept { return disk_manager_; }

} // namespace hamdb
