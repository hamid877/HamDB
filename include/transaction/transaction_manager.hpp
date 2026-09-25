#pragma once

#include "transaction/transaction.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace hamdb
{

    /**
     * @brief Manages the lifecycle of transactions.
     */
    class TransactionManager
    {
    public:
        TransactionManager() = default;
        ~TransactionManager() = default;

        TransactionManager(const TransactionManager&) = delete;
        TransactionManager& operator=(const TransactionManager&) = delete;
        TransactionManager(TransactionManager&&) = delete;
        TransactionManager& operator=(TransactionManager&&) = delete;

        /**
         * @brief Begins a new transaction.
         * @return Pointer to the newly created transaction.
         */
        [[nodiscard]] auto begin() -> Transaction*;

        /**
         * @brief Commits the given transaction.
         * @param txn The transaction to commit.
         */
        void commit(Transaction* txn);

        /**
         * @brief Aborts the given transaction.
         * @param txn The transaction to abort.
         */
        void abort(Transaction* txn);

        /**
         * @brief Retrieves an active transaction by ID.
         * @param txn_id The ID of the transaction to retrieve.
         * @return Pointer to the transaction, or nullptr if not found/active.
         */
        [[nodiscard]] auto getTransaction(txn_id_t txn_id) const -> Transaction*;

    private:
        std::atomic<txn_id_t> next_txn_id_{0};
        mutable std::shared_mutex txn_map_mutex_;
        std::unordered_map<txn_id_t, std::unique_ptr<Transaction>> active_txns_;
    };

} // namespace hamdb
