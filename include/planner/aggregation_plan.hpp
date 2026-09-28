#pragma once

#include "planner/logical_plan.hpp"
#include "planner/physical_plan.hpp"
#include "executor/aggregation_hash_table.hpp"

namespace hamdb::planner {

class LogicalAggregationNode : public LogicalPlanNode {
public:
    LogicalAggregationNode(Schema output_schema,
                           std::vector<std::unique_ptr<hamdb::Expression>> group_bys,
                           std::vector<std::unique_ptr<hamdb::Expression>> aggregates,
                           std::vector<AggregateType> agg_types);

    const std::vector<std::unique_ptr<hamdb::Expression>>& getGroupBys() const;
    std::vector<std::unique_ptr<hamdb::Expression>>& getMutableGroupBys();

    const std::vector<std::unique_ptr<hamdb::Expression>>& getAggregates() const;
    std::vector<std::unique_ptr<hamdb::Expression>>& getMutableAggregates();

    const std::vector<AggregateType>& getAggTypes() const;

private:
    std::vector<std::unique_ptr<hamdb::Expression>> group_bys_;
    std::vector<std::unique_ptr<hamdb::Expression>> aggregates_;
    std::vector<AggregateType> agg_types_;
    friend class PhysicalPlanner;
};

class AggregationPlan : public AbstractPlanNode {
public:
    AggregationPlan(Schema output_schema,
                    std::vector<std::unique_ptr<hamdb::Expression>> group_bys,
                    std::vector<std::unique_ptr<hamdb::Expression>> aggregates,
                    std::vector<AggregateType> agg_types);

    const std::vector<std::unique_ptr<hamdb::Expression>>& getGroupBys() const;
    std::vector<std::unique_ptr<hamdb::Expression>>& getMutableGroupBys();

    const std::vector<std::unique_ptr<hamdb::Expression>>& getAggregates() const;
    std::vector<std::unique_ptr<hamdb::Expression>>& getMutableAggregates();

    const std::vector<AggregateType>& getAggTypes() const;

private:
    std::vector<std::unique_ptr<hamdb::Expression>> group_bys_;
    std::vector<std::unique_ptr<hamdb::Expression>> aggregates_;
    std::vector<AggregateType> agg_types_;
};

} // namespace hamdb::planner
