#include "executor/index_scan_executor.hpp"
#include "executor/comparison_expression.hpp"
#include "executor/constant_expression.hpp"

namespace hamdb
{

    IndexScanExecutor::IndexScanExecutor(ExecutorContext* exec_ctx, const TableInfo* table_info,
                                         std::unique_ptr<Expression> predicate, std::size_t limit, std::size_t offset)
        : exec_ctx_(exec_ctx), table_info_(table_info), predicate_(std::move(predicate)), is_done_(false), limit_(limit), offset_(offset)
    {
    }

    void IndexScanExecutor::init()
    {
        is_done_ = false;
        tuples_emitted_ = 0;
        tuples_skipped_ = 0;

        if (table_info_->getIndexRootPage() != kInvalidPageId)
        {
            bplus_tree_.emplace(*exec_ctx_->getBufferPoolManager());
            bplus_tree_->open(table_info_->getIndexRootPage());
        }
        else
        {
            bplus_tree_ = std::nullopt;
        }

        auto heap_opt =
            TableHeap::open(*exec_ctx_->getDiskManager(), table_info_->getHeapRootPage());
        if (heap_opt)
        {
            table_heap_.emplace(std::move(*heap_opt));
        }
        else
        {
            table_heap_ = std::nullopt;
        }

        if (bplus_tree_ && predicate_)
        {
            auto* comp_expr = dynamic_cast<ComparisonExpression*>(predicate_.get());
            if (comp_expr)
            {
                auto* const_expr = dynamic_cast<ConstantExpression*>(comp_expr->getChildren()[1].get());
                if (const_expr)
                {
                    int64_t val = const_expr->evaluate(Tuple{}, Schema(std::vector<Column>{})).getAsInteger();
                    auto comp_type = comp_expr->getComparisonType();
                    if (comp_type == ComparisonType::Equal || comp_type == ComparisonType::GreaterThan ||
                        comp_type == ComparisonType::GreaterThanOrEqual)
                    {
                        iter_ = bplus_tree_->begin(val);
                    }
                    else if (comp_type == ComparisonType::LessThan || comp_type == ComparisonType::LessThanOrEqual)
                    {
                        iter_ = bplus_tree_->begin();
                    }
                    else
                    {
                        iter_ = bplus_tree_->end();
                    }
                }
                else
                {
                    iter_ = bplus_tree_->end();
                }
            }
            else
            {
                iter_ = bplus_tree_->end();
            }
        }
        else
        {
            iter_ = bplus_tree_->end();
        }
    }

    bool IndexScanExecutor::next(Tuple* tuple, RID* rid)
    {
        if (is_done_ || !bplus_tree_ || !table_heap_ || !iter_)
        {
            return false;
        }

        if (limit_ != std::numeric_limits<std::size_t>::max() && limit_ > 0 && tuples_emitted_ >= limit_)
        {
            return false;
        }
        if (limit_ == 0) {
            return false;
        }

        Transaction* txn = exec_ctx_->getTransaction();
        MvccManager* mvcc = exec_ctx_->getMvccManager();

        while (*iter_ != bplus_tree_->end())
        {
            auto current_rid = (**iter_).second;
            ++(*iter_);

            Tuple current_tuple;
            if (mvcc->versionCount(current_rid) > 0)
            {
                auto visible_data = mvcc->read(txn, current_rid);
                if (visible_data.has_value())
                {
                    current_tuple = Tuple(*visible_data);
                }
                else
                {
                    continue;
                }
            }
            else
            {
                Status s = table_heap_->readTuple(current_rid, current_tuple);
                if (s != Status::Ok)
                {
                    continue;
                }
            }

            if (predicate_)
            {
                Value res = predicate_->evaluate(current_tuple, table_info_->getSchema());
                if (!res.getAsBoolean())
                {
                    auto* comp_expr = dynamic_cast<ComparisonExpression*>(predicate_.get());
                    if (comp_expr)
                    {
                        auto comp_type = comp_expr->getComparisonType();
                        if (comp_type == ComparisonType::Equal || comp_type == ComparisonType::LessThan ||
                            comp_type == ComparisonType::LessThanOrEqual)
                        {
                            is_done_ = true;
                            return false;
                        }
                    }
                    continue;
                }
            }

            if (tuples_skipped_ < offset_) {
                tuples_skipped_++;
                continue;
            }

            *tuple = current_tuple;
            *rid = current_rid;
            tuples_emitted_++;
            return true;
        }

        is_done_ = true;
        return false;
    }

    const Schema& IndexScanExecutor::outputSchema() const
    {
        return table_info_->getSchema();
    }

} // namespace hamdb
