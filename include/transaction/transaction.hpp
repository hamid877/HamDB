#pragma once

#include "storage/rid.hpp"
#include <cstdint>
#include <unordered_set>
#include <functional>

namespace hamdb {

struct RIDHash {
    std::size_t operator()(const RID& rid) const noexcept {
        auto page_id = rid.getPageId();
        auto slot_id = rid.getSlotId();
        return std::hash<uint32_t>()(page_id) ^ (std::hash<uint16_t>()(slot_id) << 1);
    }
};

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

    [[nodiscard]] auto getSharedLockSet() const noexcept -> const std::unordered_set<RID, RIDHash>& { return shared_lock_set_; }
    [[nodiscard]] auto getSharedLockSet() noexcept -> std::unordered_set<RID, RIDHash>& { return shared_lock_set_; }
    
    [[nodiscard]] auto getExclusiveLockSet() const noexcept -> const std::unordered_set<RID, RIDHash>& { return exclusive_lock_set_; }
    [[nodiscard]] auto getExclusiveLockSet() noexcept -> std::unordered_set<RID, RIDHash>& { return exclusive_lock_set_; }

private:
    txn_id_t txn_id_;
    TransactionState state_;
    timestamp_t begin_ts_;
    timestamp_t commit_ts_;

    std::unordered_set<RID, RIDHash> shared_lock_set_;
    std::unordered_set<RID, RIDHash> exclusive_lock_set_;
};

} // namespace hamdb
