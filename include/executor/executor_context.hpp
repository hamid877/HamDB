#pragma once

#include "catalog/catalog_manager.hpp"
#include "transaction/transaction.hpp"
#include "transaction/mvcc_manager.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include "storage/disk_manager.hpp"

namespace hamdb {

class ExecutorContext {
public:
    ExecutorContext(Transaction* txn, CatalogManager* catalog, BufferPoolManager* bpm, MvccManager* mvcc, DiskManager* disk_manager);

    [[nodiscard]] Transaction* getTransaction() const noexcept;
    [[nodiscard]] CatalogManager* getCatalog() const noexcept;
    [[nodiscard]] BufferPoolManager* getBufferPoolManager() const noexcept;
    [[nodiscard]] MvccManager* getMvccManager() const noexcept;
    [[nodiscard]] DiskManager* getDiskManager() const noexcept;

private:
    Transaction* txn_;
    CatalogManager* catalog_;
    BufferPoolManager* bpm_;
    MvccManager* mvcc_;
    DiskManager* disk_manager_;
};

} // namespace hamdb
