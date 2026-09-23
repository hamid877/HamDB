#include "transaction/lock_manager.hpp"
#include <algorithm>
#include <cassert>

namespace hamdb {

bool LockManager::canGrantShared(const LockRequestQueue& queue, txn_id_t txn_id) const {
    if (queue.upgrading && queue.upgrading_txn != txn_id) {
        return false;
    }
    for (const auto& req : queue.request_queue) {
        if (req.txn_id == txn_id) {
            return true;
        }
        if (req.lock_mode == LockMode::EXCLUSIVE) {
            return false;
        }
    }
    return false;
}

bool LockManager::canGrantExclusive(const LockRequestQueue& queue, txn_id_t txn_id) const {
    for (const auto& req : queue.request_queue) {
        if (req.txn_id != txn_id && req.granted) {
            return false;
        }
    }
    for (const auto& req : queue.request_queue) {
        if (req.txn_id == txn_id) return true;
        return false;
    }
    return false;
}

void LockManager::grantLock(LockRequestQueue& queue, txn_id_t txn_id) {
    for (auto& req : queue.request_queue) {
        if (req.txn_id == txn_id) {
            req.granted = true;
            return;
        }
    }
}

void LockManager::removeRequest(LockRequestQueue& queue, txn_id_t txn_id) {
    auto it = std::find_if(queue.request_queue.begin(), queue.request_queue.end(),
                           [txn_id](const LockRequest& req) { return req.txn_id == txn_id; });
    if (it != queue.request_queue.end()) {
        queue.request_queue.erase(it);
    }
}

bool LockManager::lockShared(Transaction* txn, const RID& rid) {
    if (txn->getState() == TransactionState::ABORTED) {
        return false;
    }
    if (txn->getSharedLockSet().count(rid) > 0 || txn->getExclusiveLockSet().count(rid) > 0) {
        return true; // Already hold a lock
    }

    std::unique_lock<std::mutex> lock(latch_);
    auto& queue = lock_table_[rid];
    queue.request_queue.emplace_back(txn->getTransactionId(), LockMode::SHARED);

    queue.cv.wait(lock, [&]() {
        if (txn->getState() == TransactionState::ABORTED) {
            return true;
        }
        return canGrantShared(queue, txn->getTransactionId());
    });

    if (txn->getState() == TransactionState::ABORTED) {
        removeRequest(queue, txn->getTransactionId());
        queue.cv.notify_all();
        return false;
    }

    grantLock(queue, txn->getTransactionId());
    txn->getSharedLockSet().insert(rid);
    return true;
}

bool LockManager::lockExclusive(Transaction* txn, const RID& rid) {
    if (txn->getState() == TransactionState::ABORTED) {
        return false;
    }
    if (txn->getExclusiveLockSet().count(rid) > 0) {
        return true;
    }
    if (txn->getSharedLockSet().count(rid) > 0) {
        txn->setState(TransactionState::ABORTED);
        return false;
    }

    std::unique_lock<std::mutex> lock(latch_);
    auto& queue = lock_table_[rid];
    queue.request_queue.emplace_back(txn->getTransactionId(), LockMode::EXCLUSIVE);

    queue.cv.wait(lock, [&]() {
        if (txn->getState() == TransactionState::ABORTED) {
            return true;
        }
        return canGrantExclusive(queue, txn->getTransactionId());
    });

    if (txn->getState() == TransactionState::ABORTED) {
        removeRequest(queue, txn->getTransactionId());
        queue.cv.notify_all();
        return false;
    }

    grantLock(queue, txn->getTransactionId());
    txn->getExclusiveLockSet().insert(rid);
    return true;
}

bool LockManager::lockUpgrade(Transaction* txn, const RID& rid) {
    if (txn->getState() == TransactionState::ABORTED) {
        return false;
    }
    
    std::unique_lock<std::mutex> lock(latch_);
    auto& queue = lock_table_[rid];
    
    if (txn->getSharedLockSet().count(rid) == 0) {
        txn->setState(TransactionState::ABORTED);
        return false;
    }
    if (queue.upgrading) {
        txn->setState(TransactionState::ABORTED);
        return false;
    }

    queue.upgrading = true;
    queue.upgrading_txn = txn->getTransactionId();
    
    auto it = std::find_if(queue.request_queue.begin(), queue.request_queue.end(),
                           [txn](const LockRequest& req) { return req.txn_id == txn->getTransactionId(); });
    if (it != queue.request_queue.end()) {
        it->lock_mode = LockMode::EXCLUSIVE;
        it->granted = false;
    }

    txn->getSharedLockSet().erase(rid);

    queue.cv.wait(lock, [&]() {
        if (txn->getState() == TransactionState::ABORTED) {
            return true;
        }
        return canGrantExclusive(queue, txn->getTransactionId());
    });

    if (txn->getState() == TransactionState::ABORTED) {
        removeRequest(queue, txn->getTransactionId());
        queue.upgrading = false;
        queue.upgrading_txn = 0;
        queue.cv.notify_all();
        return false;
    }

    grantLock(queue, txn->getTransactionId());
    queue.upgrading = false;
    queue.upgrading_txn = 0;
    txn->getExclusiveLockSet().insert(rid);
    return true;
}

bool LockManager::unlock(Transaction* txn, const RID& rid) {
    std::unique_lock<std::mutex> lock(latch_);
    
    if (txn->getSharedLockSet().count(rid) == 0 && txn->getExclusiveLockSet().count(rid) == 0) {
        return false;
    }

    auto& queue = lock_table_[rid];
    removeRequest(queue, txn->getTransactionId());

    txn->getSharedLockSet().erase(rid);
    txn->getExclusiveLockSet().erase(rid);

    queue.cv.notify_all();
    return true;
}

void LockManager::releaseAll(Transaction* txn) {
    auto shared_locks = txn->getSharedLockSet();
    for (const auto& rid : shared_locks) {
        unlock(txn, rid);
    }
    
    auto exclusive_locks = txn->getExclusiveLockSet();
    for (const auto& rid : exclusive_locks) {
        unlock(txn, rid);
    }
}

} // namespace hamdb
