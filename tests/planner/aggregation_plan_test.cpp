#include "planner/planner.hpp"
#include "planner/aggregation_plan.hpp"
#include "planner/logical_plan.hpp"
#include "binder/bound_statement.hpp"
#include "binder/bound_expression.hpp"
#include "catalog/catalog_manager.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include "storage/disk_manager.hpp"
#include <filesystem>
#include <gtest/gtest.h>
#include <memory>

namespace hamdb::planner {

class AggregationPlanTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_ = std::filesystem::temp_directory_path() / ("test_agg_plan_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".hamdb");
        if (std::filesystem::exists(test_db_)) {
            std::filesystem::remove(test_db_);
        }
        disk_manager_ = std::make_unique<DiskManager>(test_db_);
        auto st1 = disk_manager_->createDatabase();
        EXPECT_EQ(st1, Status::Ok);
        auto st2 = disk_manager_->openDatabase();
        EXPECT_EQ(st2, Status::Ok);
        bpm_ = std::make_unique<BufferPoolManager>(10, *disk_manager_);
        catalog_ = std::make_unique<CatalogManager>(bpm_.get());
        
        std::vector<Column> cols = {
            Column("id", ColumnType::Integer),
            Column("val", ColumnType::Integer)
        };
        Schema schema(cols);
        TableInfo* info;
        auto status = catalog_->createTable("test_table", schema, info);
        EXPECT_EQ(status, Status::Ok);
        planner_ = std::make_unique<Planner>(catalog_.get());
    }
    
    void TearDown() override {
        if (std::filesystem::exists(test_db_)) {
            std::filesystem::remove(test_db_);
        }
    }
    
    std::string test_db_;
    
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> bpm_;
    std::unique_ptr<CatalogManager> catalog_;
    std::unique_ptr<Planner> planner_;
};

TEST_F(AggregationPlanTest, PlanSimpleAggregation) {
    auto stmt = std::make_unique<binder::BoundSelectStatement>();
    auto base = std::make_unique<binder::BoundBaseTableReference>();
    base->table_name_ = "test_table";
    TableInfo* info;
    (void)catalog_->getTable("test_table", info);
    base->schema_ = &info->getSchema();
    stmt->table_ = std::move(base);
    
    // SELECT id, COUNT(val) FROM test_table GROUP BY id
    
    // Group By: id
    auto gb_expr = std::make_unique<hamdb::ColumnValueExpression>(0); // idx 0
    auto bound_gb = std::make_unique<binder::BoundColumnRef>(std::move(gb_expr), TypeId::Integer, "test_table", "id");
    stmt->group_bys_.push_back(std::make_unique<binder::BoundColumnRef>(std::make_unique<hamdb::ColumnValueExpression>(0), TypeId::Integer, "test_table", "id"));
    
    // Select List: id, COUNT(val)
    stmt->select_list_.push_back(std::move(bound_gb));
    
    std::unique_ptr<hamdb::Expression> child_expr = std::make_unique<hamdb::ColumnValueExpression>(1);
    auto agg_expr = std::make_unique<binder::BoundAggregate>(std::move(child_expr), TypeId::Integer, AggregateType::Count);
    stmt->select_list_.push_back(std::move(agg_expr));
    
    auto plan = planner_->plan(std::move(stmt));
    
    ASSERT_EQ(plan->getType(), LogicalPlanType::PROJECTION);
    ASSERT_EQ(plan->getChildren().size(), 1);
    
    auto agg = plan->getChildren()[0].get();
    ASSERT_EQ(agg->getType(), LogicalPlanType::AGGREGATION);
    
    auto agg_node = static_cast<LogicalAggregationNode*>(agg);
    ASSERT_EQ(agg_node->getGroupBys().size(), 1);
    ASSERT_EQ(agg_node->getAggregates().size(), 1);
    ASSERT_EQ(agg_node->getAggTypes().size(), 1);
    ASSERT_EQ(agg_node->getAggTypes()[0], AggregateType::Count);
}

} // namespace hamdb::planner
