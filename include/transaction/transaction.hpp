#pragma once

#include <cstdint>

namespace hamdb {

/**
 * @brief Represents the state of a transaction.
 */
enum class TransactionState {
    ACTIVE,
    COMMITTED,
    ABORTED
};

using txn_id_t = uint64_t;
using timestamp_t = uint64_t;

/**
 * @brief Represents a single database transaction.
 */
class Transaction {
public:
    /**
     * @brief Constructs a new Transaction.
     * @param txn_id The unique ID for this transaction.
     */
    explicit Transaction(txn_id_t txn_id);
    ~Transaction() = default;

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction(Transaction&&) = delete;
    Transaction& operator=(Transaction&&) = delete;

    [[nodiscard]] auto getTransactionId() const noexcept -> txn_id_t { return txn_id_; }
    [[nodiscard]] auto getState() const noexcept -> TransactionState { return state_; }
    [[nodiscard]] auto getBeginTimestamp() const noexcept -> timestamp_t { return begin_ts_; }
    [[nodiscard]] auto getCommitTimestamp() const noexcept -> timestamp_t { return commit_ts_; }

    void setState(TransactionState state) noexcept { state_ = state; }
    void setBeginTimestamp(timestamp_t ts) noexcept { begin_ts_ = ts; }
    void setCommitTimestamp(timestamp_t ts) noexcept { commit_ts_ = ts; }

private:
    txn_id_t txn_id_;
    TransactionState state_;
    timestamp_t begin_ts_;
    timestamp_t commit_ts_;
};

} // namespace hamdb
