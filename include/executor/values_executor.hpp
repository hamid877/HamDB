#pragma once

#include "executor/abstract_executor.hpp"
#include "executor/expression.hpp"
#include <memory>
#include <vector>

namespace hamdb
{

    class ValuesExecutor : public AbstractExecutor
    {
    public:
        ValuesExecutor(std::vector<std::vector<std::unique_ptr<Expression>>> values,
                       Schema output_schema);

        void init() override;
        bool next(Tuple* tuple, RID* rid) override;
        [[nodiscard]] const Schema& outputSchema() const override;

    private:
        std::vector<std::vector<std::unique_ptr<Expression>>> values_;
        Schema output_schema_;
        std::size_t cursor_{0};
    };

} // namespace hamdb
