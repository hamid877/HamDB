#pragma once

/// @file mvcc_manager.hpp
/// @brief MVCC engine for snapshot-isolation reads and versioned writes.
///
/// MvccManager maintains one version chain per RID.  Every write creates
/// a new TupleVersion; commit/abort finalise or roll back the chain.
/// Snapshot isolation is enforced via TupleVersion::isVisibleTo().

#include "storage/rid.hpp"
#include "transaction/transaction.hpp"
#include "transaction/tuple_version.hpp"

#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace hamdb
{

/**
 * @brief In-memory MVCC manager providing snapshot-isolation semantics.
 *
 * ### Design
 * - One version chain per RID, stored as a singly-linked list from newest
 *   to oldest (head → prev → prev → … → nullptr).
 * - begin_txn_id / end_txn_id intervals identify the commit range during
 *   which a version is visible to snapshot readers.
 * - A global @c std::mutex serialises all write operations (insert, update,
 *   delete, commit, abort).  Reads only require a shared lock over the chain
 *   lookup, so high read concurrency is possible.
 *
 * ### Limitations (by design — out of scope)
 * - No WAL / crash recovery.
 * - No vacuum / version pruning.
 * - No serialisable isolation (write-skew detection).
 * - No predicate / gap locking.
 * - No index versioning.
 */
class MvccManager
{
public:
    MvccManager() = default;
    ~MvccManager() = default;

    MvccManager(const MvccManager&) = delete;
    MvccManager& operator=(const MvccManager&) = delete;
    MvccManager(MvccManager&&) = delete;
    MvccManager& operator=(MvccManager&&) = delete;

    // ── Write operations ──────────────────────────────────────────────────────

    /**
     * @brief Records a new insert for @p rid by transaction @p txn.
     *
     * Creates a fresh version chain head with begin_txn_id = txn->getBeginTimestamp()
     * and end_txn_id = kInfinity.  Fails if a live (non-deleted) head already
     * exists for this RID that is not owned by @p txn.
     *
     * @param txn   The writing transaction.
     * @param rid   The target record identifier.
     * @param data  Serialised tuple payload.
     * @return true on success, false when a write-write conflict is detected.
     */
    [[nodiscard]] bool insert(Transaction* txn,
                              const RID& rid,
                              std::vector<std::byte> data);

    /**
     * @brief Records an update for @p rid by transaction @p txn.
     *
     * Appends a new head version and stamps end_txn_id on the previous head.
     * Detects write-write conflicts: another uncommitted txn owns the head.
     *
     * @param txn      The writing transaction.
     * @param rid      The target record identifier.
     * @param new_data New serialised tuple payload.
     * @return true on success, false on conflict or when the tuple does not exist.
     */
    [[nodiscard]] bool update(Transaction* txn,
                              const RID& rid,
                              std::vector<std::byte> new_data);

    /**
     * @brief Records a delete for @p rid by transaction @p txn.
     *
     * Inserts a tombstone version.  Detects write-write conflicts.
     *
     * @param txn The writing transaction.
     * @param rid The target record identifier.
     * @return true on success, false on conflict or when the tuple does not exist.
     */
    [[nodiscard]] bool remove(Transaction* txn, const RID& rid);

    // ── Commit / Abort ────────────────────────────────────────────────────────

    /**
     * @brief Finalises all versions created by @p txn.
     *
     * Each version whose begin_txn_id equals txn's ID remains in the chain;
     * the commit timestamp is recorded so future snapshot readers can see the
     * committed data.  The transaction's begin_ts acts as its visible snapshot
     * ID throughout its lifetime; on commit we do not change begin_txn_id —
     * versions stay permanently visible via the original timestamp.
     *
     * For the MVCC chain, commit means: nothing needs to change in the version
     * nodes themselves because begin_txn_id already holds the stable snapshot
     * value.  The TransactionManager sets the transaction state to COMMITTED.
     *
     * @param txn The transaction to commit.
     */
    void commit(Transaction* txn);

    /**
     * @brief Rolls back all versions created by @p txn.
     *
     * Every version whose begin_txn_id equals txn->getTransactionId() is
     * removed from the chain.  If removal exposes a previous version, that
     * version's end_txn_id is restored to kTxnIdInfinity.
     *
     * @param txn The transaction to abort.
     */
    void abort(Transaction* txn);

    // ── Read operation ────────────────────────────────────────────────────────

    /**
     * @brief Returns the visible version of @p rid for transaction @p txn.
     *
     * Walks the version chain from newest to oldest looking for the first
     * version whose [begin_txn_id, end_txn_id) interval contains the
     * transaction's snapshot timestamp.
     *
     * @param txn The reading transaction.
     * @param rid The target record identifier.
     * @return The byte payload of the visible version, or std::nullopt when
     *         no visible version exists (tuple was never inserted, has been
     *         deleted, or is not yet committed for this snapshot).
     */
    [[nodiscard]] std::optional<std::vector<std::byte>> read(const Transaction* txn,
                                                             const RID& rid) const;

    /**
     * @brief Returns true if a live, visible version chain exists for @p rid.
     * @param txn The reading transaction.
     * @param rid The target record identifier.
     */
    [[nodiscard]] bool exists(const Transaction* txn, const RID& rid) const;

    /**
     * @brief Returns the number of version nodes currently stored for @p rid.
     *
     * Intended for testing and diagnostics only.
     */
    [[nodiscard]] std::size_t versionCount(const RID& rid) const;

private:
    // ── Internal helpers ──────────────────────────────────────────────────────

    /// Returns the snapshot timestamp used for reads by @p txn.
    static txn_id_t snapshotOf(const Transaction* txn) noexcept;

    /// Returns true when @p head belongs exclusively to an uncommitted txn
    /// other than @p txn_id, indicating a write-write conflict.
    static bool hasWriteConflict(const TupleVersion* head, txn_id_t txn_id) noexcept;

    // ── State ─────────────────────────────────────────────────────────────────

    mutable std::mutex latch_;

    /// Maps each RID to the head (newest) node of its version chain.
    std::unordered_map<RID, std::shared_ptr<TupleVersion>, RIDHash> chains_;
};

} // namespace hamdb
