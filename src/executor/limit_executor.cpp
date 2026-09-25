#include "executor/limit_executor.hpp"

namespace hamdb
{

    LimitExecutor::LimitExecutor(std::unique_ptr<AbstractExecutor> child, std::size_t limit, std::size_t offset)
        : child_(std::move(child)), limit_(limit), offset_(offset)
    {
    }

    void LimitExecutor::init()
    {
        child_->init();
        count_ = 0;

        Tuple dummy_tuple;
        RID dummy_rid;
        for (std::size_t i = 0; i < offset_; ++i)
        {
            if (!child_->next(&dummy_tuple, &dummy_rid))
            {
                break;
            }
        }
    }

    bool LimitExecutor::next(Tuple* tuple, RID* rid)
    {
        if (count_ >= limit_)
        {
            return false;
        }

        if (child_->next(tuple, rid))
        {
            count_++;
            return true;
        }

        return false;
    }

    const Schema& LimitExecutor::outputSchema() const
    {
        return child_->outputSchema();
    }

} // namespace hamdb
