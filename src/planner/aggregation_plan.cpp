#include "planner/aggregation_plan.hpp"

namespace hamdb::planner {

LogicalAggregationNode::LogicalAggregationNode(Schema output_schema,
                                               std::vector<std::unique_ptr<hamdb::Expression>> group_bys,
                                               std::vector<std::unique_ptr<hamdb::Expression>> aggregates,
                                               std::vector<AggregateType> agg_types)
    : LogicalPlanNode(LogicalPlanType::AGGREGATION, std::move(output_schema)),
      group_bys_(std::move(group_bys)),
      aggregates_(std::move(aggregates)),
      agg_types_(std::move(agg_types)) {}

const std::vector<std::unique_ptr<hamdb::Expression>>& LogicalAggregationNode::getGroupBys() const {
    return group_bys_;
}

std::vector<std::unique_ptr<hamdb::Expression>>& LogicalAggregationNode::getMutableGroupBys() {
    return group_bys_;
}

const std::vector<std::unique_ptr<hamdb::Expression>>& LogicalAggregationNode::getAggregates() const {
    return aggregates_;
}

std::vector<std::unique_ptr<hamdb::Expression>>& LogicalAggregationNode::getMutableAggregates() {
    return aggregates_;
}

const std::vector<AggregateType>& LogicalAggregationNode::getAggTypes() const {
    return agg_types_;
}

AggregationPlan::AggregationPlan(Schema output_schema,
                                 std::vector<std::unique_ptr<hamdb::Expression>> group_bys,
                                 std::vector<std::unique_ptr<hamdb::Expression>> aggregates,
                                 std::vector<AggregateType> agg_types)
    : AbstractPlanNode(PhysicalPlanType::AGGREGATION, std::move(output_schema)),
      group_bys_(std::move(group_bys)),
      aggregates_(std::move(aggregates)),
      agg_types_(std::move(agg_types)) {}

const std::vector<std::unique_ptr<hamdb::Expression>>& AggregationPlan::getGroupBys() const {
    return group_bys_;
}

std::vector<std::unique_ptr<hamdb::Expression>>& AggregationPlan::getMutableGroupBys() {
    return group_bys_;
}

const std::vector<std::unique_ptr<hamdb::Expression>>& AggregationPlan::getAggregates() const {
    return aggregates_;
}

std::vector<std::unique_ptr<hamdb::Expression>>& AggregationPlan::getMutableAggregates() {
    return aggregates_;
}

const std::vector<AggregateType>& AggregationPlan::getAggTypes() const {
    return agg_types_;
}

} // namespace hamdb::planner
