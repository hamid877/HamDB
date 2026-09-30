#pragma once

#include "catalog/statistics_manager.hpp"
#include "executor/expression.hpp"
#include <memory>
#include <unordered_map>

namespace hamdb
{

    class CardinalityEstimator
    {
    public:
        explicit CardinalityEstimator(StatisticsManager* stats_manager)
            : stats_manager_(stats_manager)
        {
        }

        [[nodiscard]] double estimateSelectivity(const Expression* predicate,
                                                 const TableStatistics& stats,
                                                 const Schema& schema) const;

        [[nodiscard]] double estimateJoinSelectivity(const Expression* predicate,
                                                     const TableStatistics& left_stats,
                                                     const Schema& left_schema,
                                                     const TableStatistics& right_stats,
                                                     const Schema& right_schema) const;

    private:
        StatisticsManager* stats_manager_;
    };

} // namespace hamdb
