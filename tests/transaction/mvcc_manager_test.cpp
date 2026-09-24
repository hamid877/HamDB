#include "transaction/mvcc_manager.hpp"
#include "transaction/transaction_manager.hpp"
#include "transaction/tuple_version.hpp"
#include "storage/rid.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

namespace hamdb
{
namespace
{

// ── Helpers ────────────────────────────────────────────────────────────────

/// Build a tiny byte vector from a string literal for easy test payloads.
std::vector<std::byte> makePayload(std::string_view s)
{
    std::vector<std::byte> out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<std::byte>(c));
    }
    return out;
}

bool payloadEq(const std::vector<std::byte>& a, std::string_view b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (static_cast<char>(a[i]) != b[i]) {
            return false;
        }
    }
    return true;
}

// ── TupleVersion unit tests ────────────────────────────────────────────────

TEST(TupleVersionTest, DefaultConstruct)
{
    TupleVersion v;
    EXPECT_EQ(v.begin_txn_id, TupleVersion::kTxnIdInfinity);
    EXPECT_EQ(v.end_txn_id, TupleVersion::kTxnIdInfinity);
    EXPECT_FALSE(v.is_committed);
    EXPECT_FALSE(v.is_deleted);
    EXPECT_TRUE(v.data.empty());
    EXPECT_EQ(v.prev, nullptr);
}

TEST(TupleVersionTest, IsVisibleTo_ExactRange)
{
    TupleVersion v;
    v.begin_txn_id = 5;
    v.end_txn_id = 10;
    v.is_committed = true;

    EXPECT_TRUE(v.isVisibleTo(5));
    EXPECT_TRUE(v.isVisibleTo(7));
    EXPECT_TRUE(v.isVisibleTo(9));
    EXPECT_FALSE(v.isVisibleTo(4));  // before begin
    EXPECT_FALSE(v.isVisibleTo(10)); // at end (exclusive)
    EXPECT_FALSE(v.isVisibleTo(11));
}

TEST(TupleVersionTest, IsVisibleTo_InfiniteEnd)
{
    TupleVersion v;
    v.begin_txn_id = 3;
    v.end_txn_id = TupleVersion::kTxnIdInfinity;
    v.is_committed = true;

    EXPECT_TRUE(v.isVisibleTo(3));
    EXPECT_TRUE(v.isVisibleTo(1000));
    EXPECT_FALSE(v.isVisibleTo(2)); // before begin
}

TEST(TupleVersionTest, ParameterisedConstruct)
{
    auto prev = std::make_shared<TupleVersion>();
    TupleVersion v(7, makePayload("hello"), false, prev);

    EXPECT_EQ(v.begin_txn_id, 7U);
    EXPECT_EQ(v.end_txn_id, TupleVersion::kTxnIdInfinity);
    EXPECT_FALSE(v.is_committed);
    EXPECT_FALSE(v.is_deleted);
    EXPECT_TRUE(payloadEq(v.data, "hello"));
    EXPECT_EQ(v.prev, prev);
}

// ── MvccManager: basic insert & read ──────────────────────────────────────

class MvccManagerTest : public ::testing::Test
{
protected:
    TransactionManager txn_mgr_;
    MvccManager mvcc_;
    RID rid1_{1, 0};
    RID rid2_{1, 1};
    RID rid3_{2, 0};
};

TEST_F(MvccManagerTest, InsertAndReadOwnWrite)
{
    auto* txn = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.insert(txn, rid1_, makePayload("row1")));

    auto val = mvcc_.read(txn, rid1_);
    ASSERT_TRUE(val.has_value());
    EXPECT_TRUE(payloadEq(*val, "row1"));

    txn_mgr_.commit(txn);
}

TEST_F(MvccManagerTest, ReadNonExistentRid)
{
    auto* txn = txn_mgr_.begin();
    EXPECT_FALSE(mvcc_.read(txn, rid1_).has_value());
    txn_mgr_.commit(txn);
}

TEST_F(MvccManagerTest, InsertDuplicateRidSameTxnFails)
{
    auto* txn = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.insert(txn, rid1_, makePayload("v1")));
    // Second insert into the same RID by the same txn must fail.
    EXPECT_FALSE(mvcc_.insert(txn, rid1_, makePayload("v2")));
    txn_mgr_.commit(txn);
}

TEST_F(MvccManagerTest, CommittedInsertVisibleToLaterSnapshot)
{
    // Txn A inserts and commits.
    auto* txnA = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.insert(txnA, rid1_, makePayload("committed")));
    mvcc_.commit(txnA);
    txn_mgr_.commit(txnA);

    // Txn B begins AFTER A committed → should see A's data.
    auto* txnB = txn_mgr_.begin();
    auto val = mvcc_.read(txnB, rid1_);
    ASSERT_TRUE(val.has_value());
    EXPECT_TRUE(payloadEq(*val, "committed"));
    txn_mgr_.commit(txnB);
}

TEST_F(MvccManagerTest, UncommittedInsertNotVisibleToOtherSnapshot)
{
    // Txn A inserts but has NOT yet committed.
    auto* txnA = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.insert(txnA, rid1_, makePayload("uncommitted")));

    // Txn B starts while A is still active.
    auto* txnB = txn_mgr_.begin();
    // B should NOT see A's uncommitted write.
    EXPECT_FALSE(mvcc_.read(txnB, rid1_).has_value());

    txn_mgr_.commit(txnA);
    txn_mgr_.commit(txnB);
}

// ── Update ────────────────────────────────────────────────────────────────

TEST_F(MvccManagerTest, UpdateCreatesNewVersion)
{
    auto* txnA = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.insert(txnA, rid1_, makePayload("v1")));
    mvcc_.commit(txnA);
    txn_mgr_.commit(txnA);

    auto* txnB = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.update(txnB, rid1_, makePayload("v2")));
    // B reads its own update.
    auto val = mvcc_.read(txnB, rid1_);
    ASSERT_TRUE(val.has_value());
    EXPECT_TRUE(payloadEq(*val, "v2"));
    // Two versions: v2 (head) + v1 (prev).
    EXPECT_EQ(mvcc_.versionCount(rid1_), 2U);
    mvcc_.commit(txnB);
    txn_mgr_.commit(txnB);
}

TEST_F(MvccManagerTest, OldSnapshotSeesPreUpdateVersion)
{
    auto* txnA = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.insert(txnA, rid1_, makePayload("original")));
    mvcc_.commit(txnA);
    txn_mgr_.commit(txnA);

    // C begins before the update.
    auto* txnC = txn_mgr_.begin();

    auto* txnB = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.update(txnB, rid1_, makePayload("updated")));
    mvcc_.commit(txnB);
    txn_mgr_.commit(txnB);

    // C's snapshot was taken before B committed → must see "original".
    auto val = mvcc_.read(txnC, rid1_);
    ASSERT_TRUE(val.has_value());
    EXPECT_TRUE(payloadEq(*val, "original"));
    txn_mgr_.commit(txnC);
}

TEST_F(MvccManagerTest, UpdateNonExistentRidFails)
{
    auto* txn = txn_mgr_.begin();
    EXPECT_FALSE(mvcc_.update(txn, rid1_, makePayload("x")));
    txn_mgr_.commit(txn);
}

// ── Delete ────────────────────────────────────────────────────────────────

TEST_F(MvccManagerTest, DeleteMakesTupleInvisible)
{
    auto* txnA = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.insert(txnA, rid1_, makePayload("to_delete")));
    mvcc_.commit(txnA);
    txn_mgr_.commit(txnA);

    auto* txnB = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.remove(txnB, rid1_));
    // B itself sees the tuple as deleted.
    EXPECT_FALSE(mvcc_.read(txnB, rid1_).has_value());
    mvcc_.commit(txnB);
    txn_mgr_.commit(txnB);

    // New txn also cannot see it.
    auto* txnC = txn_mgr_.begin();
    EXPECT_FALSE(mvcc_.read(txnC, rid1_).has_value());
    txn_mgr_.commit(txnC);
}

TEST_F(MvccManagerTest, OldSnapshotSeesDeletedTuple)
{
    auto* txnA = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.insert(txnA, rid1_, makePayload("alive")));
    mvcc_.commit(txnA);
    txn_mgr_.commit(txnA);

    // C begins before the delete.
    auto* txnC = txn_mgr_.begin();

    auto* txnB = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.remove(txnB, rid1_));
    mvcc_.commit(txnB);
    txn_mgr_.commit(txnB);

    // C should still see "alive".
    auto val = mvcc_.read(txnC, rid1_);
    ASSERT_TRUE(val.has_value());
    EXPECT_TRUE(payloadEq(*val, "alive"));
    txn_mgr_.commit(txnC);
}

TEST_F(MvccManagerTest, DeleteNonExistentRidFails)
{
    auto* txn = txn_mgr_.begin();
    EXPECT_FALSE(mvcc_.remove(txn, rid1_));
    txn_mgr_.commit(txn);
}

// ── Abort ─────────────────────────────────────────────────────────────────

TEST_F(MvccManagerTest, AbortedInsertNotVisible)
{
    auto* txnA = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.insert(txnA, rid1_, makePayload("ghost")));
    mvcc_.abort(txnA);
    txn_mgr_.abort(txnA);

    auto* txnB = txn_mgr_.begin();
    EXPECT_FALSE(mvcc_.read(txnB, rid1_).has_value());
    txn_mgr_.commit(txnB);
}

TEST_F(MvccManagerTest, AbortedUpdateRestoresOldVersion)
{
    auto* txnA = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.insert(txnA, rid1_, makePayload("original")));
    mvcc_.commit(txnA);
    txn_mgr_.commit(txnA);

    auto* txnB = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.update(txnB, rid1_, makePayload("updated")));
    // B reads updated.
    auto beforeAbort = mvcc_.read(txnB, rid1_);
    ASSERT_TRUE(beforeAbort.has_value());
    EXPECT_TRUE(payloadEq(*beforeAbort, "updated"));

    mvcc_.abort(txnB);
    txn_mgr_.abort(txnB);

    auto* txnC = txn_mgr_.begin();
    auto afterAbort = mvcc_.read(txnC, rid1_);
    ASSERT_TRUE(afterAbort.has_value());
    EXPECT_TRUE(payloadEq(*afterAbort, "original"));
    txn_mgr_.commit(txnC);
}

TEST_F(MvccManagerTest, AbortedDeleteRestoresTuple)
{
    auto* txnA = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.insert(txnA, rid1_, makePayload("survivor")));
    mvcc_.commit(txnA);
    txn_mgr_.commit(txnA);

    auto* txnB = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.remove(txnB, rid1_));
    EXPECT_FALSE(mvcc_.read(txnB, rid1_).has_value()); // B sees deletion
    mvcc_.abort(txnB);
    txn_mgr_.abort(txnB);

    auto* txnC = txn_mgr_.begin();
    auto val = mvcc_.read(txnC, rid1_);
    ASSERT_TRUE(val.has_value());
    EXPECT_TRUE(payloadEq(*val, "survivor"));
    txn_mgr_.commit(txnC);
}

// ── Write-write conflict detection ────────────────────────────────────────

TEST_F(MvccManagerTest, WriteWriteConflictOnUpdate)
{
    auto* txnA = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.insert(txnA, rid1_, makePayload("v1")));
    mvcc_.commit(txnA);
    txn_mgr_.commit(txnA);

    // Two concurrent writers; B starts first.
    auto* txnB = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.update(txnB, rid1_, makePayload("byB")));

    // C tries to update the same RID while B has not committed.
    auto* txnC = txn_mgr_.begin();
    EXPECT_FALSE(mvcc_.update(txnC, rid1_, makePayload("byC")));

    mvcc_.commit(txnB);
    txn_mgr_.commit(txnB);
    txn_mgr_.abort(txnC);
}

TEST_F(MvccManagerTest, WriteWriteConflictOnDelete)
{
    auto* txnA = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.insert(txnA, rid1_, makePayload("v1")));
    mvcc_.commit(txnA);
    txn_mgr_.commit(txnA);

    auto* txnB = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.remove(txnB, rid1_));

    auto* txnC = txn_mgr_.begin();
    EXPECT_FALSE(mvcc_.remove(txnC, rid1_));

    mvcc_.commit(txnB);
    txn_mgr_.commit(txnB);
    txn_mgr_.abort(txnC);
}

// ── Version chain length ──────────────────────────────────────────────────

TEST_F(MvccManagerTest, VersionCountGrowsWithUpdates)
{
    auto* txnA = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.insert(txnA, rid1_, makePayload("v1")));
    mvcc_.commit(txnA);
    txn_mgr_.commit(txnA);
    EXPECT_EQ(mvcc_.versionCount(rid1_), 1U);

    auto* txnB = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.update(txnB, rid1_, makePayload("v2")));
    mvcc_.commit(txnB);
    txn_mgr_.commit(txnB);
    EXPECT_EQ(mvcc_.versionCount(rid1_), 2U);

    auto* txnC = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.update(txnC, rid1_, makePayload("v3")));
    mvcc_.commit(txnC);
    txn_mgr_.commit(txnC);
    EXPECT_EQ(mvcc_.versionCount(rid1_), 3U);
}

TEST_F(MvccManagerTest, VersionCountZeroForUnknownRid)
{
    EXPECT_EQ(mvcc_.versionCount(rid1_), 0U);
}

// ── Exists helper ─────────────────────────────────────────────────────────

TEST_F(MvccManagerTest, ExistsFalseBeforeInsert)
{
    auto* txn = txn_mgr_.begin();
    EXPECT_FALSE(mvcc_.exists(txn, rid1_));
    txn_mgr_.commit(txn);
}

TEST_F(MvccManagerTest, ExistsTrueAfterInsert)
{
    auto* txnA = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.insert(txnA, rid1_, makePayload("x")));
    EXPECT_TRUE(mvcc_.exists(txnA, rid1_));
    mvcc_.commit(txnA);
    txn_mgr_.commit(txnA);
}

TEST_F(MvccManagerTest, ExistsFalseAfterDelete)
{
    auto* txnA = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.insert(txnA, rid1_, makePayload("x")));
    mvcc_.commit(txnA);
    txn_mgr_.commit(txnA);

    auto* txnB = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.remove(txnB, rid1_));
    EXPECT_FALSE(mvcc_.exists(txnB, rid1_));
    mvcc_.commit(txnB);
    txn_mgr_.commit(txnB);
}

// ── Multiple RIDs ─────────────────────────────────────────────────────────

TEST_F(MvccManagerTest, MultipleRidsIsolated)
{
    auto* txn = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.insert(txn, rid1_, makePayload("A")));
    ASSERT_TRUE(mvcc_.insert(txn, rid2_, makePayload("B")));
    ASSERT_TRUE(mvcc_.insert(txn, rid3_, makePayload("C")));

    EXPECT_TRUE(payloadEq(*mvcc_.read(txn, rid1_), "A"));
    EXPECT_TRUE(payloadEq(*mvcc_.read(txn, rid2_), "B"));
    EXPECT_TRUE(payloadEq(*mvcc_.read(txn, rid3_), "C"));

    mvcc_.commit(txn);
    txn_mgr_.commit(txn);
}

// ── Tombstone is NOT visible as data ─────────────────────────────────────

TEST_F(MvccManagerTest, TombstoneNotReadableAsData)
{
    auto* txnA = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.insert(txnA, rid1_, makePayload("alive")));
    mvcc_.commit(txnA);
    txn_mgr_.commit(txnA);

    auto* txnB = txn_mgr_.begin();
    ASSERT_TRUE(mvcc_.remove(txnB, rid1_));
    // Even within the deleting txn, read should return nullopt.
    EXPECT_FALSE(mvcc_.read(txnB, rid1_).has_value());
    mvcc_.commit(txnB);
    txn_mgr_.commit(txnB);
}

} // namespace
} // namespace hamdb
