#include "planner/planner.hpp"
#include "planner/having_plan.hpp"
#include "planner/aggregation_plan.hpp"
#include "binder/bound_statement.hpp"
#include "binder/bound_expression.hpp"
#include "catalog/catalog_manager.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include "storage/disk_manager.hpp"
#include <filesystem>
#include <gtest/gtest.h>
#include <memory>

namespace hamdb::planner {

class HavingPlanTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_ = std::filesystem::temp_directory_path() / ("test_having_plan_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".hamdb");
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

TEST_F(HavingPlanTest, PlanHaving) {
    auto stmt = std::make_unique<binder::BoundSelectStatement>();
    auto base = std::make_unique<binder::BoundBaseTableReference>();
    base->table_name_ = "test_table";
    TableInfo* info;
    (void)catalog_->getTable("test_table", info);
    base->schema_ = &info->getSchema();
    stmt->table_ = std::move(base);
    
    auto gb_expr = std::make_unique<hamdb::ColumnValueExpression>(0); // idx 0
    auto bound_gb = std::make_unique<binder::BoundColumnRef>(std::move(gb_expr), TypeId::Integer, "test_table", "id");
    stmt->group_bys_.push_back(std::make_unique<binder::BoundColumnRef>(std::make_unique<hamdb::ColumnValueExpression>(0), TypeId::Integer, "test_table", "id"));
    stmt->select_list_.push_back(std::move(bound_gb));
    
    auto agg_expr = std::make_unique<binder::BoundAggregate>(std::make_unique<hamdb::ColumnValueExpression>(1), TypeId::Integer, AggregateType::Count);
    stmt->select_list_.push_back(std::make_unique<binder::BoundAggregate>(std::make_unique<hamdb::ColumnValueExpression>(1), TypeId::Integer, AggregateType::Count));
    
    // HAVING
    auto cmp_expr = std::make_unique<hamdb::ComparisonExpression>(ComparisonType::GreaterThan, std::make_unique<hamdb::ColumnValueExpression>(1), std::make_unique<hamdb::ConstantExpression>(Value(5)));
    stmt->having_clause_ = std::make_unique<binder::BoundComparison>(std::move(cmp_expr), TypeId::Boolean);

    auto plan = planner_->plan(std::move(stmt));
    
    ASSERT_EQ(plan->getType(), LogicalPlanType::PROJECTION);
    ASSERT_EQ(plan->getChildren().size(), 1);
    
    auto having = plan->getChildren()[0].get();
    ASSERT_EQ(having->getType(), LogicalPlanType::HAVING);
    
    auto having_node = static_cast<LogicalHavingNode*>(having);
    ASSERT_NE(having_node->getPredicate(), nullptr);
    
    auto agg = having_node->getChildren()[0].get();
    ASSERT_EQ(agg->getType(), LogicalPlanType::AGGREGATION);
}

} // namespace hamdb::planner
