#include "transaction/mvcc_manager.hpp"

namespace hamdb
{

    // ── Private static helpers ────────────────────────────────────────────────────

    txn_id_t MvccManager::snapshotOf(const Transaction* txn) noexcept
    {
        // We use the transaction's begin timestamp as its stable snapshot ID.
        // TransactionManager::begin() assigns begin_ts = txn_id monotonically.
        return txn->getBeginTimestamp();
    }

    bool MvccManager::hasWriteConflict(const TupleVersion* head, txn_id_t txn_id) noexcept
    {
        if (head == nullptr)
        {
            return false;
        }
        // A write-write conflict exists when the head was written by a *different*
        // transaction that has NOT yet committed (is_committed == false).
        // Committed heads are safe targets for new writes regardless of who wrote them.
        return !head->is_committed && head->begin_txn_id != txn_id;
    }

    // ── Write operations ──────────────────────────────────────────────────────────

    bool MvccManager::insert(Transaction* txn, const RID& rid, std::vector<std::byte> data)
    {
        std::lock_guard lock(latch_);

        auto it = chains_.find(rid);
        if (it != chains_.end())
        {
            const auto* head = it->second.get();
            if (head != nullptr && !head->is_deleted)
            {
                // Conflict if a live version exists from another uncommitted txn.
                if (hasWriteConflict(head, txn->getTransactionId()))
                {
                    return false;
                }
                // Same txn re-inserting an already-live version fails.
                if (head->begin_txn_id == txn->getTransactionId() && !head->is_committed)
                {
                    return false;
                }
                // A committed live head means the tuple already exists.
                if (head->is_committed)
                {
                    return false;
                }
            }
        }

        auto version = std::make_shared<TupleVersion>(txn->getTransactionId(), std::move(data),
                                                      false, nullptr);

        chains_[rid] = std::move(version);
        return true;
    }

    bool MvccManager::update(Transaction* txn, const RID& rid, std::vector<std::byte> new_data)
    {
        std::lock_guard lock(latch_);

        auto it = chains_.find(rid);
        if (it == chains_.end())
        {
            return false; // tuple does not exist
        }

        auto& head = it->second;
        if (head == nullptr || head->is_deleted)
        {
            return false; // logically deleted
        }

        if (hasWriteConflict(head.get(), txn->getTransactionId()))
        {
            return false; // write-write conflict
        }

        // If this txn already wrote the current head, allow update-chaining
        // (re-stamp end to ourself to rebuild the chain).
        head->end_txn_id = txn->getTransactionId();

        auto new_version =
            std::make_shared<TupleVersion>(txn->getTransactionId(), std::move(new_data), false,
                                           head); // link old head as prev

        it->second = std::move(new_version);
        return true;
    }

    bool MvccManager::remove(Transaction* txn, const RID& rid)
    {
        std::lock_guard lock(latch_);

        auto it = chains_.find(rid);
        if (it == chains_.end())
        {
            return false; // tuple does not exist
        }

        auto& head = it->second;
        if (head == nullptr || head->is_deleted)
        {
            return false; // already deleted
        }

        if (hasWriteConflict(head.get(), txn->getTransactionId()))
        {
            return false; // write-write conflict
        }

        // Stamp the outgoing head.
        head->end_txn_id = txn->getTransactionId();

        // Insert a tombstone as the new head (uncommitted).
        auto tombstone = std::make_shared<TupleVersion>(txn->getTransactionId(),
                                                        std::vector<std::byte>{}, true, head);

        it->second = std::move(tombstone);
        return true;
    }

    // ── Commit / Abort ────────────────────────────────────────────────────────────

    void MvccManager::commit(Transaction* txn)
    {
        std::lock_guard lock(latch_);

        const txn_id_t tid = txn->getTransactionId();

        // Mark every version owned by this txn as committed so that other
        // snapshots can now see them via the isVisibleTo() predicate.
        for (auto& [rid, head] : chains_)
        {
            TupleVersion* cur = head.get();
            while (cur != nullptr)
            {
                if (cur->begin_txn_id == tid)
                {
                    cur->is_committed = true;
                }
                cur = cur->prev.get();
            }
        }
    }

    void MvccManager::abort(Transaction* txn)
    {
        std::lock_guard lock(latch_);

        const txn_id_t tid = txn->getTransactionId();

        // Walk every chain and remove all versions owned by this txn.
        for (auto& [rid, head] : chains_)
        {
            // Traverse newest-to-oldest, skip nodes with begin_txn_id == tid.
            std::vector<std::shared_ptr<TupleVersion>> nodes;
            {
                std::shared_ptr<TupleVersion> cur = head;
                while (cur != nullptr)
                {
                    nodes.push_back(cur);
                    cur = cur->prev;
                }
            }

            // Rebuild the chain without our versions.
            std::shared_ptr<TupleVersion> new_head;
            std::shared_ptr<TupleVersion>* attach = &new_head;
            for (auto& node : nodes)
            {
                if (node->begin_txn_id == tid)
                {
                    continue; // drop this version
                }
                auto kept = std::make_shared<TupleVersion>();
                kept->begin_txn_id = node->begin_txn_id;
                kept->end_txn_id = node->end_txn_id;
                kept->is_committed = node->is_committed;
                kept->is_deleted = node->is_deleted;
                kept->data = node->data;
                *attach = kept;
                attach = &kept->prev;
            }

            // Restore the new head's end_txn_id if we were the ones who stamped it.
            if (new_head != nullptr && new_head->end_txn_id == tid)
            {
                new_head->end_txn_id = TupleVersion::kTxnIdInfinity;
            }

            head = new_head;
        }
    }

    // ── Read operation ────────────────────────────────────────────────────────────

    std::optional<std::vector<std::byte>> MvccManager::read(const Transaction* txn,
                                                            const RID& rid) const
    {
        std::lock_guard lock(latch_);

        auto it = chains_.find(rid);
        if (it == chains_.end())
        {
            return std::nullopt;
        }

        const txn_id_t snap = snapshotOf(txn);
        const txn_id_t tid = txn->getTransactionId();

        // Walk newest-to-oldest looking for the first visible or own-write version.
        const TupleVersion* cur = it->second.get();
        while (cur != nullptr)
        {
            // A txn can always read its own uncommitted writes.
            bool own_write = (!cur->is_committed && cur->begin_txn_id == tid);
            // Committed versions are visible if within the snapshot range.
            bool visible = cur->isVisibleTo(snap);

            if (own_write || visible)
            {
                if (cur->is_deleted)
                {
                    return std::nullopt; // logically deleted
                }
                return cur->data;
            }
            cur = cur->prev.get();
        }

        return std::nullopt;
    }

    bool MvccManager::exists(const Transaction* txn, const RID& rid) const
    {
        return read(txn, rid).has_value();
    }

    std::size_t MvccManager::versionCount(const RID& rid) const
    {
        std::lock_guard lock(latch_);

        auto it = chains_.find(rid);
        if (it == chains_.end())
        {
            return 0;
        }

        std::size_t count = 0;
        const TupleVersion* cur = it->second.get();
        while (cur != nullptr)
        {
            ++count;
            cur = cur->prev.get();
        }
        return count;
    }

} // namespace hamdb
