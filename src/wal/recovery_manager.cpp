#include "wal/recovery_manager.hpp"
#include "storage/slotted_page.hpp"
#include <algorithm>

namespace hamdb
{

    RecoveryManager::RecoveryManager(BufferPoolManager& bpm, LogManager& log_manager)
        : bpm_(bpm), log_manager_(log_manager)
    {
    }

    void RecoveryManager::recover()
    {
        log_records_ = log_manager_.getDiskLogBuffer();
        if (log_records_.empty())
            return;

        analyze();
        redo();
        undo();
    }

    void RecoveryManager::analyze()
    {
        for (const auto& record : log_records_)
        {
            if (record.getRecordType() == LogRecordType::BEGIN)
            {
                active_txns_.insert(record.getTxnId());
            }
            else if (record.getRecordType() == LogRecordType::COMMIT ||
                     record.getRecordType() == LogRecordType::ABORT)
            {
                active_txns_.erase(record.getTxnId());
            }
        }
    }

    void RecoveryManager::redo()
    {
        for (const auto& record : log_records_)
        {
            if (record.getRecordType() == LogRecordType::INSERT ||
                record.getRecordType() == LogRecordType::UPDATE ||
                record.getRecordType() == LogRecordType::DELETE)
            {

                auto rid = record.getRID();
                WritePageGuard guard;
                if (bpm_.fetchPageWrite(rid.getPageId(), guard) != Status::Ok)
                    continue;

                if (guard.pageMut().header().page_lsn >= record.getLSN())
                {
                    continue;
                }

                SlottedPage slotted_page(guard.pageMut());

                if (record.getRecordType() == LogRecordType::INSERT)
                {
                    (void)slotted_page.insertTupleAtSlot(rid.getSlotId(), record.getTuple());
                }
                else if (record.getRecordType() == LogRecordType::UPDATE)
                {
                    (void)slotted_page.updateTuple(rid.getSlotId(), record.getNewTuple());
                }
                else if (record.getRecordType() == LogRecordType::DELETE)
                {
                    (void)slotted_page.deleteTuple(rid.getSlotId());
                }

                guard.pageMut().header().page_lsn = record.getLSN();
            }
        }
    }

    void RecoveryManager::undo()
    {
        for (auto it = log_records_.rbegin(); it != log_records_.rend(); ++it)
        {
            const auto& record = *it;
            if (active_txns_.find(record.getTxnId()) == active_txns_.end())
            {
                continue;
            }

            if (record.getRecordType() == LogRecordType::INSERT ||
                record.getRecordType() == LogRecordType::UPDATE ||
                record.getRecordType() == LogRecordType::DELETE)
            {

                auto rid = record.getRID();
                WritePageGuard guard;
                if (bpm_.fetchPageWrite(rid.getPageId(), guard) != Status::Ok)
                    continue;

                SlottedPage slotted_page(guard.pageMut());

                if (record.getRecordType() == LogRecordType::INSERT)
                {
                    (void)slotted_page.deleteTuple(rid.getSlotId());
                }
                else if (record.getRecordType() == LogRecordType::UPDATE)
                {
                    (void)slotted_page.updateTuple(rid.getSlotId(), record.getOldTuple());
                }
                else if (record.getRecordType() == LogRecordType::DELETE)
                {
                    (void)slotted_page.insertTupleAtSlot(rid.getSlotId(), record.getTuple());
                }
            }
        }
    }

} // namespace hamdb
