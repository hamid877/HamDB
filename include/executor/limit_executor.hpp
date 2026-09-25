#pragma once

#include "executor/abstract_executor.hpp"
#include <cstddef>
#include <memory>

namespace hamdb
{

    class LimitExecutor : public AbstractExecutor
    {
    public:
        LimitExecutor(std::unique_ptr<AbstractExecutor> child, std::size_t limit, std::size_t offset);

        void init() override;
        bool next(Tuple* tuple, RID* rid) override;
        const Schema& outputSchema() const override;

    private:
        std::unique_ptr<AbstractExecutor> child_;
        std::size_t limit_;
        std::size_t offset_;
        std::size_t count_{0};
    };

} // namespace hamdb
