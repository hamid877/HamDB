#include "planner/planner.hpp"
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

class PlannerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_ = "test_planner.hamdb";
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

TEST_F(PlannerTest, PlanSelectWithFilterAndLimit) {
    auto stmt = std::make_unique<binder::BoundSelectStatement>();
    stmt->table_name_ = "test_table";
    
    auto col_expr = std::make_unique<hamdb::ColumnValueExpression>(0);
    auto bound_col = std::make_unique<binder::BoundColumnRef>(std::move(col_expr), TypeId::Integer, "test_table", "id");
    stmt->select_list_.push_back(std::move(bound_col));
    
    auto const_expr = std::make_unique<hamdb::ConstantExpression>(Value(5));
    auto bound_const = std::make_unique<binder::BoundConstant>(std::move(const_expr), TypeId::Integer);
    stmt->where_clause_ = std::move(bound_const);
    
    auto limit_expr = std::make_unique<hamdb::ConstantExpression>(Value(10));
    auto bound_limit = std::make_unique<binder::BoundConstant>(std::move(limit_expr), TypeId::Integer);
    stmt->limit_ = std::move(bound_limit);

    auto plan = planner_->plan(std::move(stmt));
    
    ASSERT_EQ(plan->getType(), LogicalPlanType::LIMIT);
    ASSERT_EQ(plan->getChildren().size(), 1);
    
    auto proj = plan->getChildren()[0].get();
    ASSERT_EQ(proj->getType(), LogicalPlanType::PROJECTION);
    ASSERT_EQ(proj->getChildren().size(), 1);
    
    auto filter = proj->getChildren()[0].get();
    ASSERT_EQ(filter->getType(), LogicalPlanType::FILTER);
    ASSERT_EQ(filter->getChildren().size(), 1);
    
    auto seq = filter->getChildren()[0].get();
    ASSERT_EQ(seq->getType(), LogicalPlanType::SEQ_SCAN);
    
    auto seq_scan = static_cast<SeqScanPlanNode*>(seq);
    ASSERT_EQ(seq_scan->getTableName(), "test_table");
}

TEST_F(PlannerTest, PlanInsertValues) {
    auto stmt = std::make_unique<binder::BoundInsertStatement>();
    stmt->table_name_ = "test_table";
    
    std::vector<std::unique_ptr<binder::BoundExpression>> row;
    auto const_expr = std::make_unique<hamdb::ConstantExpression>(Value(1));
    row.push_back(std::make_unique<binder::BoundConstant>(std::move(const_expr), TypeId::Integer));
    stmt->values_.push_back(std::move(row));
    
    auto plan = planner_->plan(std::move(stmt));
    ASSERT_EQ(plan->getType(), LogicalPlanType::INSERT);
    ASSERT_EQ(plan->getOutputSchema().getColumnCount(), 1);
    ASSERT_EQ(plan->getOutputSchema().getColumn(0).getName(), "affected_rows");
    
    ASSERT_EQ(plan->getChildren().size(), 1);
    auto values = plan->getChildren()[0].get();
    ASSERT_EQ(values->getType(), LogicalPlanType::VALUES);
    ASSERT_EQ(static_cast<ValuesPlanNode*>(values)->getValues().size(), 1);
}

} // namespace hamdb::planner
