#include "transaction/transaction.hpp"

namespace hamdb
{

    Transaction::Transaction(txn_id_t txn_id)
        : txn_id_(txn_id), state_(TransactionState::ACTIVE), begin_ts_(0), commit_ts_(0)
    {
    }

} // namespace hamdb
