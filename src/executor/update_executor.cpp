#include "executor/update_executor.hpp"
#include "executor/column_value_expression.hpp"
#include "utils/serializer.hpp"
#include "wal/log_record.hpp"
#include <iostream>

namespace hamdb
{

    UpdateExecutor::UpdateExecutor(ExecutorContext* exec_ctx, const TableInfo* table_info,
                                   std::unique_ptr<AbstractExecutor> child_executor,
                                   std::vector<std::unique_ptr<Expression>> target_expressions)
        : exec_ctx_(exec_ctx), table_info_(table_info), child_executor_(std::move(child_executor)),
          target_expressions_(std::move(target_expressions))
    {
        std::vector<Column> cols;
        cols.emplace_back("count", ColumnType::Integer);
        output_schema_ = Schema(std::move(cols));
    }

    void UpdateExecutor::init()
    {
        is_done_ = false;
        child_executor_->init();

        if (table_info_->getIndexRootPage() != kInvalidPageId)
        {
            bplus_tree_.emplace(*exec_ctx_->getBufferPoolManager());
            bplus_tree_->open(table_info_->getIndexRootPage());
        }
        else
        {
            bplus_tree_ = std::nullopt;
        }
    }

    bool UpdateExecutor::next(Tuple* tuple, RID* rid)
    {
        if (is_done_)
        {
            return false;
        }

        int32_t count = 0;
        Tuple child_tuple;
        RID child_rid;

        Transaction* txn = exec_ctx_->getTransaction();
        MvccManager* mvcc = exec_ctx_->getMvccManager();
        LockManager* lock_mgr = exec_ctx_->getLockManager();
        LogManager* log_mgr = exec_ctx_->getLogManager();

        while (child_executor_->next(&child_tuple, &child_rid))
        {
            // Acquire exclusive lock
            lock_mgr->lockExclusive(txn, child_rid);

            // Evaluate target expressions
            std::vector<Value> new_values;
            new_values.reserve(target_expressions_.size());
            for (const auto& expr : target_expressions_)
            {
                new_values.push_back(expr->evaluate(child_tuple, table_info_->getSchema()));
            }

            // Serialize new values into a new Tuple
            std::vector<std::byte> buf(1024);
            Serializer ser(buf);
            for (size_t i = 0; i < new_values.size(); ++i)
            {
                auto type = table_info_->getSchema().getColumn(i).getType();
                if (type == ColumnType::Integer)
                {
                    (void)ser.writeInt32(new_values[i].getAsInteger());
                }
                else if (type == ColumnType::Boolean)
                {
                    (void)ser.writeBool(new_values[i].getAsBoolean());
                }
                else if (type == ColumnType::Varchar)
                {
                    (void)ser.writeString(new_values[i].getAsVarchar());
                }
            }
            Tuple new_tuple(std::span<const std::byte>(buf.data(), ser.position()));

            // Tombstone old version and insert new version via MVCC update
            std::vector<std::byte> new_data(new_tuple.data().begin(), new_tuple.data().end());
            if (!mvcc->update(txn, child_rid, std::move(new_data)))
            {
                continue; // conflict or missing
            }

            // Update primary B+ Tree index if indexed column (col 0) changed
            if (bplus_tree_)
            {
                int64_t old_key = ColumnValueExpression(0)
                                      .evaluate(child_tuple, table_info_->getSchema())
                                      .getAsInteger();
                int64_t new_key = new_values[0].getAsInteger();
                if (old_key != new_key)
                {
                    (void)bplus_tree_->remove(old_key);
                    (void)bplus_tree_->insert(new_key, child_rid);
                }
            }

            // Emit WAL UPDATE record
            LogRecord log(txn->getTransactionId(), LogRecordType::UPDATE, kInvalidLSN, child_rid,
                          child_tuple, new_tuple);
            log_mgr->append(log);

            count++;
        }

        std::vector<std::byte> buf(8);
        Serializer ser(buf);
        (void)ser.writeInt32(count);
        *tuple = Tuple(std::span<const std::byte>(buf.data(), ser.position()));
        *rid = RID(); // invalid rid

        is_done_ = true;
        return true;
    }

    const Schema& UpdateExecutor::outputSchema() const
    {
        return output_schema_;
    }

} // namespace hamdb
