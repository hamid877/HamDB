#include "transaction/transaction.hpp"
#include "transaction/transaction_manager.hpp"

#include <gtest/gtest.h>

namespace hamdb
{

    TEST(TransactionTest, BeginTransaction)
    {
        TransactionManager txn_mgr;
        auto* txn = txn_mgr.begin();

        EXPECT_NE(txn, nullptr);
        EXPECT_EQ(txn->getState(), TransactionState::ACTIVE);
        EXPECT_EQ(txn->getTransactionId(), 0);
        EXPECT_EQ(txn->getBeginTimestamp(), 0);

        auto* fetched_txn = txn_mgr.getTransaction(0);
        EXPECT_EQ(txn, fetched_txn);
    }

    TEST(TransactionTest, CommitTransaction)
    {
        TransactionManager txn_mgr;
        auto* txn = txn_mgr.begin();
        txn_id_t id = txn->getTransactionId();

        // Test transaction state before commit
        EXPECT_EQ(txn->getState(), TransactionState::ACTIVE);
        EXPECT_EQ(txn_mgr.getTransaction(id), txn);

        txn_mgr.commit(txn);

        // The transaction should be removed from active transactions
        EXPECT_EQ(txn_mgr.getTransaction(id), nullptr);
    }

    TEST(TransactionTest, AbortTransaction)
    {
        TransactionManager txn_mgr;
        auto* txn = txn_mgr.begin();
        txn_id_t id = txn->getTransactionId();

        // Test transaction state before abort
        EXPECT_EQ(txn->getState(), TransactionState::ACTIVE);
        EXPECT_EQ(txn_mgr.getTransaction(id), txn);

        txn_mgr.abort(txn);

        // The transaction should be removed from active transactions
        EXPECT_EQ(txn_mgr.getTransaction(id), nullptr);
    }

    TEST(TransactionTest, MonotonicallyIncreasingIds)
    {
        TransactionManager txn_mgr;
        auto* txn1 = txn_mgr.begin();
        auto* txn2 = txn_mgr.begin();
        auto* txn3 = txn_mgr.begin();

        EXPECT_LT(txn1->getTransactionId(), txn2->getTransactionId());
        EXPECT_LT(txn2->getTransactionId(), txn3->getTransactionId());

        EXPECT_EQ(txn1->getTransactionId(), 0);
        EXPECT_EQ(txn2->getTransactionId(), 1);
        EXPECT_EQ(txn3->getTransactionId(), 2);
    }

} // namespace hamdb
