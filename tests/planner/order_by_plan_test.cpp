#include <gtest/gtest.h>
#include "planner/order_by_plan.hpp"
#include "executor/column_value_expression.hpp"

namespace hamdb::planner::test {

TEST(OrderByPlanTest, LogicalOrderByNodeCreation) {
    Schema schema({Column{"col1", ColumnType::Integer}});
    std::vector<std::pair<std::unique_ptr<hamdb::Expression>, bool>> order_bys;
    order_bys.emplace_back(std::make_unique<ColumnValueExpression>(0), true);
    
    LogicalOrderByNode node(schema, std::move(order_bys));
    EXPECT_EQ(node.getType(), LogicalPlanType::ORDER_BY);
    EXPECT_EQ(node.getOrderBy().size(), 1);
}

TEST(OrderByPlanTest, PhysicalOrderByPlanCreation) {
    Schema schema({Column{"col1", ColumnType::Integer}});
    std::vector<std::pair<OrderByDirection, std::unique_ptr<hamdb::Expression>>> order_bys;
    order_bys.emplace_back(OrderByDirection::ASC, std::make_unique<ColumnValueExpression>(0));
    
    OrderByPlan plan(schema, std::move(order_bys));
    EXPECT_EQ(plan.getType(), PhysicalPlanType::ORDER_BY);
    EXPECT_EQ(plan.getOrderBy().size(), 1);
}

} // namespace hamdb::planner::test
