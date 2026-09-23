#pragma once

#include "storage/rid.hpp"
#include "transaction/transaction.hpp"
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <deque>

namespace hamdb {

enum class LockMode {
    SHARED,
    EXCLUSIVE
};

struct LockRequest {
    txn_id_t txn_id;
    LockMode lock_mode;
    bool granted{false};
    
    LockRequest(txn_id_t txn_id, LockMode lock_mode)
        : txn_id(txn_id), lock_mode(lock_mode) {}
};

struct LockRequestQueue {
    std::condition_variable cv;
    std::deque<LockRequest> request_queue;
    bool upgrading{false};
    txn_id_t upgrading_txn{0};
};

class LockManager {
public:
    LockManager() = default;
    ~LockManager() = default;
    
    // Disable copy/move
    LockManager(const LockManager&) = delete;
    LockManager& operator=(const LockManager&) = delete;
    LockManager(LockManager&&) = delete;
    LockManager& operator=(LockManager&&) = delete;

    bool lockShared(Transaction* txn, const RID& rid);
    bool lockExclusive(Transaction* txn, const RID& rid);
    bool lockUpgrade(Transaction* txn, const RID& rid);
    bool unlock(Transaction* txn, const RID& rid);
    void releaseAll(Transaction* txn);

private:
    bool canGrantShared(const LockRequestQueue& queue, txn_id_t txn_id) const;
    bool canGrantExclusive(const LockRequestQueue& queue, txn_id_t txn_id) const;
    void grantLock(LockRequestQueue& queue, txn_id_t txn_id);
    void removeRequest(LockRequestQueue& queue, txn_id_t txn_id);

    std::mutex latch_;
    std::unordered_map<RID, LockRequestQueue, RIDHash> lock_table_;
};

} // namespace hamdb
