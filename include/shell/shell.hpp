#pragma once

#include <string>
#include <memory>
#include <vector>
#include <iostream>

#include "catalog/catalog_manager.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include "storage/disk_manager.hpp"
#include "transaction/transaction_manager.hpp"
#include "transaction/lock_manager.hpp"
#include "transaction/mvcc_manager.hpp"
#include "wal/log_manager.hpp"
#include "planner/planner.hpp"
#include "planner/physical_planner.hpp"
#include "optimizer/optimizer.hpp"
#include "executor/executor_context.hpp"
#include "prepared/prepared_statement_manager.hpp"

namespace hamdb::shell {

class Shell {
public:
    explicit Shell(const std::string& db_name);
    ~Shell();

    void executeSQL(const std::string& query, std::ostream& out);
    void executeMeta(const std::string& cmd, std::ostream& out);

    CatalogManager* getCatalog() const { return catalog_.get(); }
    BufferPoolManager* getBufferPoolManager() const { return bpm_.get(); }
    DiskManager* getDiskManager() const { return disk_manager_.get(); }
    TransactionManager* getTransactionManager() const { return txn_manager_.get(); }
    PreparedStatementManager* getPreparedStatementManager() const { return prep_manager_.get(); }

private:
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> bpm_;
    std::unique_ptr<CatalogManager> catalog_;
    std::unique_ptr<LogManager> log_manager_;
    std::unique_ptr<LockManager> lock_manager_;
    std::unique_ptr<MvccManager> mvcc_manager_;
    std::unique_ptr<TransactionManager> txn_manager_;
    
    std::unique_ptr<planner::Planner> planner_;
    std::unique_ptr<planner::PhysicalPlanner> physical_planner_;
    std::unique_ptr<optimizer::IOptimizer> optimizer_;
    std::unique_ptr<PreparedStatementManager> prep_manager_;
    
    void initDB(const std::string& db_name);
};

} // namespace hamdb::shell
