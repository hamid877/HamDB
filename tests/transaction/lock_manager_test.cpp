#include "transaction/lock_manager.hpp"
#include "transaction/transaction_manager.hpp"
#include "storage/rid.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

namespace hamdb {

TEST(LockManagerTest, BasicSharedLock) {
    LockManager lock_mgr;
    TransactionManager txn_mgr;
    
    auto txn1 = txn_mgr.begin();
    auto txn2 = txn_mgr.begin();
    RID rid(1, 0);

    EXPECT_TRUE(lock_mgr.lockShared(txn1, rid));
    EXPECT_TRUE(lock_mgr.lockShared(txn2, rid));

    EXPECT_TRUE(lock_mgr.unlock(txn1, rid));
    EXPECT_TRUE(lock_mgr.unlock(txn2, rid));
}

TEST(LockManagerTest, BasicExclusiveLock) {
    LockManager lock_mgr;
    TransactionManager txn_mgr;
    
    auto txn1 = txn_mgr.begin();
    RID rid(1, 0);

    EXPECT_TRUE(lock_mgr.lockExclusive(txn1, rid));
    EXPECT_TRUE(lock_mgr.unlock(txn1, rid));
}

TEST(LockManagerTest, ExclusiveBlocksShared) {
    LockManager lock_mgr;
    TransactionManager txn_mgr;
    
    auto txn1 = txn_mgr.begin();
    auto txn2 = txn_mgr.begin();
    RID rid(1, 0);

    EXPECT_TRUE(lock_mgr.lockExclusive(txn1, rid));

    bool thread_finished = false;
    std::thread t([&]() {
        EXPECT_TRUE(lock_mgr.lockShared(txn2, rid));
        thread_finished = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(thread_finished);

    lock_mgr.unlock(txn1, rid);
    t.join();
    EXPECT_TRUE(thread_finished);
}

TEST(LockManagerTest, LockUpgrade) {
    LockManager lock_mgr;
    TransactionManager txn_mgr;
    
    auto txn1 = txn_mgr.begin();
    RID rid(1, 0);

    EXPECT_TRUE(lock_mgr.lockShared(txn1, rid));
    EXPECT_TRUE(lock_mgr.lockUpgrade(txn1, rid));

    EXPECT_TRUE(lock_mgr.unlock(txn1, rid));
}

TEST(LockManagerTest, AbortReleaseAll) {
    LockManager lock_mgr;
    TransactionManager txn_mgr;
    
    auto txn1 = txn_mgr.begin();
    RID rid1(1, 0);
    RID rid2(1, 1);

    EXPECT_TRUE(lock_mgr.lockShared(txn1, rid1));
    EXPECT_TRUE(lock_mgr.lockExclusive(txn1, rid2));

    txn_mgr.abort(txn1);
    lock_mgr.releaseAll(txn1);

    EXPECT_TRUE(txn1->getSharedLockSet().empty());
    EXPECT_TRUE(txn1->getExclusiveLockSet().empty());
}

} // namespace hamdb
