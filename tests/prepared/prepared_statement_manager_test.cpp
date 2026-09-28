#include <gtest/gtest.h>
#include "prepared/prepared_statement_manager.hpp"

using namespace hamdb;
using namespace hamdb::planner;

TEST(PreparedStatementManagerTest, BasicTest) {
    PreparedStatementManager manager;
    
    EXPECT_EQ(manager.getStatement("stmt1"), nullptr);
    
    auto stmt1 = std::make_unique<PreparedStatement>("stmt1", nullptr, nullptr, std::vector<TypeId>{});
    manager.addStatement(std::move(stmt1));
    
    EXPECT_NE(manager.getStatement("stmt1"), nullptr);
    EXPECT_EQ(manager.getStatement("stmt1")->getName(), "stmt1");
    
    manager.removeStatement("stmt1");
    EXPECT_EQ(manager.getStatement("stmt1"), nullptr);
    
    auto stmt2 = std::make_unique<PreparedStatement>("stmt2", nullptr, nullptr, std::vector<TypeId>{});
    manager.addStatement(std::move(stmt2));
    manager.clear();
    EXPECT_EQ(manager.getStatement("stmt2"), nullptr);
}
