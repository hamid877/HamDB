#pragma once

#include "common/enums.hpp"
#include "storage/rid.hpp"
#include "storage/tuple.hpp"
#include "transaction/transaction.hpp"
#include "utils/serializer.hpp"
#include "utils/deserializer.hpp"
#include <cstdint>
#include <string_view>

namespace hamdb {

using lsn_t = uint64_t;
inline constexpr lsn_t kInvalidLSN = 0;

enum class LogRecordType : uint8_t {
    BEGIN = 0,
    INSERT = 1,
    UPDATE = 2,
    DELETE = 3,
    COMMIT = 4,
    ABORT = 5
};

/**
 * @brief Represents a single write-ahead log record.
 */
class LogRecord {
public:
    LogRecord() = default;

    // BEGIN, COMMIT, ABORT
    LogRecord(txn_id_t txn_id, LogRecordType record_type, lsn_t prev_lsn);

    // INSERT, DELETE
    LogRecord(txn_id_t txn_id, LogRecordType record_type, lsn_t prev_lsn, RID rid, const Tuple& tuple);

    // UPDATE
    LogRecord(txn_id_t txn_id, LogRecordType record_type, lsn_t prev_lsn, RID rid, const Tuple& old_tuple, const Tuple& new_tuple);

    [[nodiscard]] auto getLSN() const noexcept -> lsn_t { return lsn_; }
    [[nodiscard]] auto getTxnId() const noexcept -> txn_id_t { return txn_id_; }
    [[nodiscard]] auto getRecordType() const noexcept -> LogRecordType { return record_type_; }
    [[nodiscard]] auto getPrevLSN() const noexcept -> lsn_t { return prev_lsn_; }
    [[nodiscard]] auto getRID() const noexcept -> RID { return rid_; }
    [[nodiscard]] auto getTuple() const noexcept -> const Tuple& { return tuple_; }
    [[nodiscard]] auto getOldTuple() const noexcept -> const Tuple& { return old_tuple_; }
    [[nodiscard]] auto getNewTuple() const noexcept -> const Tuple& { return new_tuple_; }

    void setLSN(lsn_t lsn) noexcept { lsn_ = lsn; }

    [[nodiscard]] auto getSerializedSize() const noexcept -> uint32_t { return size_; }
    [[nodiscard]] Status serialize(Serializer& serializer) const noexcept;
    [[nodiscard]] Status deserialize(Deserializer& deserializer) noexcept;

    [[nodiscard]] static std::string_view recordTypeToString(LogRecordType type) noexcept;

private:
    uint32_t size_{0};
    LogRecordType record_type_{LogRecordType::BEGIN};
    txn_id_t txn_id_{0};
    lsn_t prev_lsn_{kInvalidLSN};
    lsn_t lsn_{kInvalidLSN};

    RID rid_{};
    Tuple old_tuple_{};
    Tuple new_tuple_{};
    Tuple tuple_{}; 
};

} // namespace hamdb
