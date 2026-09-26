#include "catalog/catalog_manager.hpp"
#include "catalog/table_info.hpp"
#include "planner/executor_factory.hpp"
#include "planner/physical_planner.hpp"
#include "planner/planner.hpp"
#include "parser/parser.hpp"
#include "binder/binder.hpp"
#include "storage/table_heap.hpp"
#include "index/bplus_tree.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include "storage/disk_manager.hpp"
#include "transaction/transaction.hpp"
#include "transaction/transaction_manager.hpp"
#include "transaction/lock_manager.hpp"
#include "transaction/mvcc_manager.hpp"
#include "wal/log_manager.hpp"
#include <filesystem>
#include <gtest/gtest.h>
#include <memory>

namespace hamdb::planner {

class ExecutorFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_ = "test_executor_factory.hamdb";
        if (std::filesystem::exists(test_db_)) {
            std::filesystem::remove(test_db_);
        }
        disk_manager_ = std::make_unique<DiskManager>(test_db_);
        auto st1 = disk_manager_->createDatabase();
        EXPECT_EQ(st1, Status::Ok);
        auto st2 = disk_manager_->openDatabase();
        EXPECT_EQ(st2, Status::Ok);
        bpm_ = std::make_unique<BufferPoolManager>(10, *disk_manager_);
        catalog_ = std::make_unique<CatalogManager>(bpm_.get());
        
        log_manager_ = std::make_unique<LogManager>();
        lock_manager_ = std::make_unique<LockManager>();
        mvcc_manager_ = std::make_unique<MvccManager>();
        txn_mgr_ = std::make_unique<TransactionManager>();
        
        txn_ = txn_mgr_->begin();
        exec_ctx_ = std::make_unique<ExecutorContext>(
            txn_, catalog_.get(), bpm_.get(),
            mvcc_manager_.get(), disk_manager_.get(),
            lock_manager_.get(), log_manager_.get()
        );
        
        std::vector<Column> cols = {
            Column("id", ColumnType::Integer),
            Column("val", ColumnType::Integer)
        };
        Schema schema(cols);
        
        TableInfo* info;
        auto st3 = catalog_->createTable("test_table", schema, info);
        EXPECT_EQ(st3, Status::Ok);

        struct TableInfoHack {
            std::uint32_t table_id_;
            std::string table_name_;
            PageId heap_root_page_;
            PageId index_root_page_;
            Schema schema_;
        };

        struct BPlusTreeLayout {
            BufferPoolManager* bpm_;
            PageId root_page_id_;
        };

        auto heap = TableHeap::create(*disk_manager_);
        BPlusTree tree(*bpm_);
        tree.create();
        PageId tree_root = reinterpret_cast<BPlusTreeLayout*>(&tree)->root_page_id_;

        auto* hack = reinterpret_cast<TableInfoHack*>(info);
        hack->heap_root_page_ = heap->getFirstPageId();
        hack->index_root_page_ = tree_root;

        planner_ = std::make_unique<Planner>(catalog_.get());
        physical_planner_ = std::make_unique<PhysicalPlanner>(catalog_.get());
    }
    
    void TearDown() override {
        if (std::filesystem::exists(test_db_)) {
            std::filesystem::remove(test_db_);
        }
    }
    
    std::string test_db_;
    
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> bpm_;
    std::unique_ptr<CatalogManager> catalog_;
    std::unique_ptr<LogManager> log_manager_;
    std::unique_ptr<LockManager> lock_manager_;
    std::unique_ptr<MvccManager> mvcc_manager_;
    std::unique_ptr<TransactionManager> txn_mgr_;
    Transaction* txn_;
    std::unique_ptr<ExecutorContext> exec_ctx_;
    
    std::unique_ptr<Planner> planner_;
    std::unique_ptr<PhysicalPlanner> physical_planner_;
};

TEST_F(ExecutorFactoryTest, EndToEndExecution) {
    auto execute = [&](const std::string& query) {
        Parser parser(query);
        auto ast = parser.parseStatement();
        binder::Binder binder(catalog_.get());
        auto bound_stmt = binder.bind(*ast);
        auto logical_plan = planner_->plan(std::move(bound_stmt));
        auto physical_plan = physical_planner_->plan(std::move(logical_plan));
        auto exec = ExecutorFactory::createExecutor(exec_ctx_.get(), std::move(physical_plan));
        exec->init();
        Tuple tuple;
        RID rid;
        int count = 0;
        while (exec->next(&tuple, &rid)) {
            count++;
        }
        return count;
    };

    EXPECT_EQ(execute("INSERT INTO test_table VALUES (1, 10), (2, 20)"), 1); // 1 affected row tuple
    EXPECT_EQ(execute("SELECT * FROM test_table"), 2); // 2 rows retrieved
    EXPECT_EQ(execute("UPDATE test_table SET val = 15 WHERE id = 1"), 1); // 1 affected row
    EXPECT_EQ(execute("DELETE FROM test_table WHERE id = 2"), 1); // 1 affected row
    EXPECT_EQ(execute("SELECT * FROM test_table"), 1); // 1 row remaining
}

} // namespace hamdb::planner
