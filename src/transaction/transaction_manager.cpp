#include "transaction/transaction_manager.hpp"

namespace hamdb
{

    auto TransactionManager::begin() -> Transaction*
    {
        auto txn_id = next_txn_id_.fetch_add(1, std::memory_order_relaxed);
        auto txn = std::make_unique<Transaction>(txn_id);
        txn->setBeginTimestamp(txn_id); // Using txn_id as a simple timestamp for now

        auto* txn_ptr = txn.get();

        std::unique_lock lock(txn_map_mutex_);
        active_txns_[txn_id] = std::move(txn);

        return txn_ptr;
    }

    void TransactionManager::commit(Transaction* txn)
    {
        if (txn == nullptr)
        {
            return;
        }

        std::unique_lock lock(txn_map_mutex_);
        auto it = active_txns_.find(txn->getTransactionId());
        if (it != active_txns_.end())
        {
            it->second->setState(TransactionState::COMMITTED);
            it->second->setCommitTimestamp(next_txn_id_.fetch_add(1, std::memory_order_relaxed));

            // Remove from active transactions, which triggers RAII cleanup of the transaction
            // object
            active_txns_.erase(it);
        }
    }

    void TransactionManager::abort(Transaction* txn)
    {
        if (txn == nullptr)
        {
            return;
        }

        std::unique_lock lock(txn_map_mutex_);
        auto it = active_txns_.find(txn->getTransactionId());
        if (it != active_txns_.end())
        {
            it->second->setState(TransactionState::ABORTED);

            // Remove from active transactions, which triggers RAII cleanup of the transaction
            // object
            active_txns_.erase(it);
        }
    }

    auto TransactionManager::getTransaction(txn_id_t txn_id) const -> Transaction*
    {
        std::shared_lock lock(txn_map_mutex_);
        auto it = active_txns_.find(txn_id);
        if (it != active_txns_.end())
        {
            return it->second.get();
        }
        return nullptr;
    }

} // namespace hamdb
