#pragma once

#include "executor/abstract_executor.hpp"
#include "executor/aggregation_hash_table.hpp"
#include "executor/expression.hpp"
#include <memory>
#include <vector>

namespace hamdb
{

    class AggregationExecutor : public AbstractExecutor
    {
    public:
        AggregationExecutor(std::unique_ptr<AbstractExecutor> child,
                            std::vector<std::unique_ptr<Expression>> group_bys,
                            std::vector<std::unique_ptr<Expression>> aggregates,
                            std::vector<AggregateType> agg_types, Schema output_schema);

        void init() override;
        bool next(Tuple* tuple, RID* rid) override;
        const Schema& outputSchema() const override;

    private:
        std::unique_ptr<AbstractExecutor> child_;
        std::vector<std::unique_ptr<Expression>> group_bys_;
        std::vector<std::unique_ptr<Expression>> aggregates_;
        std::vector<AggregateType> agg_types_;
        Schema output_schema_;

        AggregationHashTable aht_;
        std::unique_ptr<AggregationHashTable::Iterator> aht_iterator_;
        bool is_successful_{false}; // Used to handle empty table case if no GROUP BY
    };

} // namespace hamdb
