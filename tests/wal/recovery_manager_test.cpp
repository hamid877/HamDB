#include "wal/recovery_manager.hpp"
#include "wal/log_manager.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include "storage/disk_manager.hpp"
#include "storage/slotted_page.hpp"
#include "common/constants.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <vector>

namespace hamdb {

class RecoveryManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path_ = "recovery_test.hamdb";
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove(test_db_path_);
        }
        
        disk_manager_ = std::make_unique<DiskManager>(test_db_path_);
        EXPECT_EQ(disk_manager_->createDatabase(), Status::Ok);
        EXPECT_EQ(disk_manager_->openDatabase(), Status::Ok);
        
        bpm_ = std::make_unique<BufferPoolManager>(10, *disk_manager_);
        log_manager_ = std::make_unique<LogManager>();
    }

    void TearDown() override {
        bpm_.reset();
        (void)disk_manager_->closeDatabase();
        disk_manager_.reset();
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove(test_db_path_);
        }
    }

    std::filesystem::path test_db_path_;
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> bpm_;
    std::unique_ptr<LogManager> log_manager_;
};

TEST_F(RecoveryManagerTest, RedoCommittedInsert) {
    PageId page_id;
    EXPECT_EQ(disk_manager_->allocatePage(page_id), Status::Ok);
    
    // Initialize slotted page
    {
        WritePageGuard guard;
        EXPECT_EQ(bpm_->fetchPageWrite(page_id, guard), Status::Ok);
        SlottedPage sp(guard.pageMut());
        EXPECT_EQ(sp.initialize(), Status::Ok);
    }

    Tuple tuple(std::vector<std::byte>{std::byte{1}, std::byte{2}, std::byte{3}});
    RID rid(page_id, 0);

    // Simulate logs
    LogRecord begin(1, LogRecordType::BEGIN, kInvalidLSN);
    LogRecord insert(1, LogRecordType::INSERT, begin.getLSN(), rid, tuple);
    LogRecord commit(1, LogRecordType::COMMIT, insert.getLSN());
    
    log_manager_->append(begin);
    log_manager_->append(insert);
    log_manager_->append(commit);
    log_manager_->flushAll();

    RecoveryManager rm(*bpm_, *log_manager_);
    rm.recover();

    // Verify
    ReadPageGuard guard;
    EXPECT_EQ(bpm_->fetchPageRead(page_id, guard), Status::Ok);
    SlottedPage sp(const_cast<Page&>(guard.page())); 
    EXPECT_EQ(sp.tupleCount(), 1);
    
    Tuple read_t;
    EXPECT_EQ(sp.readTuple(0, read_t), Status::Ok);
    EXPECT_EQ(read_t.size(), tuple.size());
}

TEST_F(RecoveryManagerTest, UndoUncommittedInsert) {
    PageId page_id;
    EXPECT_EQ(disk_manager_->allocatePage(page_id), Status::Ok);
    
    // Initialize slotted page and insert a tuple manually
    Tuple tuple(std::vector<std::byte>{std::byte{1}, std::byte{2}, std::byte{3}});
    RID rid(page_id, 0);

    {
        WritePageGuard guard;
        EXPECT_EQ(bpm_->fetchPageWrite(page_id, guard), Status::Ok);
        SlottedPage sp(guard.pageMut());
        EXPECT_EQ(sp.initialize(), Status::Ok);
        SlotId slot;
        EXPECT_EQ(sp.insertTuple(tuple, slot), Status::Ok);
        EXPECT_EQ(slot, 0);
        guard.pageMut().header().page_lsn = 2; // Simulate it was written
    }

    // Simulate logs for uncommitted tx
    LogRecord begin(1, LogRecordType::BEGIN, kInvalidLSN);
    LogRecord insert(1, LogRecordType::INSERT, begin.getLSN(), rid, tuple);
    // No commit!
    
    log_manager_->append(begin);
    log_manager_->append(insert);
    log_manager_->flushAll();

    RecoveryManager rm(*bpm_, *log_manager_);
    rm.recover();

    // Verify it was undone (deleted)
    ReadPageGuard guard;
    EXPECT_EQ(bpm_->fetchPageRead(page_id, guard), Status::Ok);
    SlottedPage sp(const_cast<Page&>(guard.page()));
    Tuple read_t;
    EXPECT_EQ(sp.readTuple(0, read_t), Status::InvalidArg); // Deleted
}

} // namespace hamdb
