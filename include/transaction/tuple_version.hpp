#pragma once

/// @file tuple_version.hpp
/// @brief Version metadata for a single tuple version in the MVCC version chain.

#include "storage/rid.hpp"
#include "transaction/transaction.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace hamdb
{

    /**
     * @brief Immutable snapshot of a tuple at a particular point in time.
     *
     * Each write (insert, update, delete) creates a new TupleVersion node.
     * Versions are chained from newest to oldest via the @c prev pointer.
     *
     * ### Visibility model
     *
     * A version transitions through two phases:
     *
     * 1. **Uncommitted** (`is_committed == false`): Created by an active txn.
     *    Only the owning txn can read it.  Other txns must not see it and
     *    must treat it as a write-write conflict when writing.
     *
     * 2. **Committed** (`is_committed == true`): The owning txn has committed.
     *    The version becomes visible to any snapshot whose timestamp satisfies
     *    `begin_txn_id <= snapshot_ts < end_txn_id`.
     *
     * ### Lifecycle
     *   - Created by txn T: `begin_txn_id = T.id`, `is_committed = false`.
     *   - T commits: head version's `is_committed` set to `true`.
     *     The previous head's `end_txn_id` is set to `T.id` at write time.
     *   - T aborts: version is removed from the chain entirely.
     */
    struct TupleVersion
    {
        /// Sentinel meaning "visible forever" or "not yet ended".
        static constexpr txn_id_t kTxnIdInfinity = UINT64_MAX;

        // ── Version metadata ──────────────────────────────────────────────────────

        /// ID of the transaction that created this version.
        txn_id_t begin_txn_id{kTxnIdInfinity};

        /// ID of the first transaction that superseded this version.
        /// kTxnIdInfinity means the version is the current head.
        txn_id_t end_txn_id{kTxnIdInfinity};

        /// True when the writing transaction has committed this version.
        bool is_committed{false};

        /// True when this version represents a DELETE marker (tombstone).
        bool is_deleted{false};

        // ── Payload ───────────────────────────────────────────────────────────────

        /// Byte payload of the tuple.  Empty when is_deleted is true.
        std::vector<std::byte> data;

        // ── Version chain ─────────────────────────────────────────────────────────

        /// Pointer to the immediately preceding version (older), or nullptr.
        std::shared_ptr<TupleVersion> prev;

        // ── Construction ──────────────────────────────────────────────────────────

        TupleVersion() = default;

        /**
         * @brief Constructs an uncommitted version.
         * @param begin  Transaction ID that created this version.
         * @param payload Serialised tuple bytes (may be empty for tombstones).
         * @param deleted True when this is a delete marker.
         * @param previous Pointer to the previous version in the chain.
         */
        TupleVersion(txn_id_t begin, std::vector<std::byte> payload, bool deleted,
                     std::shared_ptr<TupleVersion> previous)
            : begin_txn_id(begin), end_txn_id(kTxnIdInfinity), is_deleted(deleted),
              data(std::move(payload)), prev(std::move(previous))
        {
        }

        TupleVersion(const TupleVersion&) = delete;
        TupleVersion& operator=(const TupleVersion&) = delete;
        TupleVersion(TupleVersion&&) = default;
        TupleVersion& operator=(TupleVersion&&) = default;
        ~TupleVersion() = default;

        // ── Visibility predicate ──────────────────────────────────────────────────

        /**
         * @brief Returns true if this committed version is visible to a snapshot
         *        taken at @p snapshot_ts (snapshot-isolation read).
         *
         * A committed version [B, E) is visible to a reader with snapshot
         * timestamp S when:  B <= S  &&  E > S
         *
         * Uncommitted versions are never visible through this predicate; they are
         * only accessible to their own writing transaction directly.
         *
         * @param snapshot_ts The begin_txn_id of the reading transaction.
         */
        [[nodiscard]] bool isVisibleTo(txn_id_t snapshot_ts) const noexcept
        {
            return is_committed && begin_txn_id <= snapshot_ts && snapshot_ts < end_txn_id;
        }
    };

} // namespace hamdb
