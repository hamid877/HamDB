#include <gtest/gtest.h>
#include "prepared/prepared_statement.hpp"
#include "planner/logical_plan.hpp"
#include "planner/physical_plan.hpp"

using namespace hamdb;
using namespace hamdb::planner;

TEST(PreparedStatementTest, BasicTest) {
    auto schema = Schema(std::vector<Column>{});
    auto logical = std::make_unique<SeqScanPlanNode>(schema, "t1", "t1");
    auto physical = std::make_unique<SeqScanPlan>(schema, "t1", "t1");
    std::vector<TypeId> params = {TypeId::Integer, TypeId::Varchar};
    
    PreparedStatement stmt("stmt1", std::move(logical), std::move(physical), params);
    
    EXPECT_EQ(stmt.getName(), "stmt1");
    EXPECT_EQ(stmt.getParameterTypes().size(), 2);
    EXPECT_EQ(stmt.getParameterTypes()[0], TypeId::Integer);
    EXPECT_EQ(stmt.getParameterTypes()[1], TypeId::Varchar);
    EXPECT_NE(stmt.getLogicalPlan(), nullptr);
    EXPECT_NE(stmt.getPhysicalPlan(), nullptr);
}
