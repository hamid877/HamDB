#include "executor/executor_context.hpp"

namespace hamdb
{

    ExecutorContext::ExecutorContext(Transaction* txn, CatalogManager* catalog,
                                     BufferPoolManager* bpm, MvccManager* mvcc,
                                     DiskManager* disk_manager, LockManager* lock_mgr,
                                     LogManager* log_mgr)
        : txn_(txn), catalog_(catalog), bpm_(bpm), mvcc_(mvcc), disk_manager_(disk_manager),
          lock_mgr_(lock_mgr), log_mgr_(log_mgr)
    {
    }

    Transaction* ExecutorContext::getTransaction() const noexcept
    {
        return txn_;
    }
    CatalogManager* ExecutorContext::getCatalog() const noexcept
    {
        return catalog_;
    }
    BufferPoolManager* ExecutorContext::getBufferPoolManager() const noexcept
    {
        return bpm_;
    }
    MvccManager* ExecutorContext::getMvccManager() const noexcept
    {
        return mvcc_;
    }
    DiskManager* ExecutorContext::getDiskManager() const noexcept
    {
        return disk_manager_;
    }
    LockManager* ExecutorContext::getLockManager() const noexcept
    {
        return lock_mgr_;
    }
    LogManager* ExecutorContext::getLogManager() const noexcept
    {
        return log_mgr_;
    }

} // namespace hamdb
