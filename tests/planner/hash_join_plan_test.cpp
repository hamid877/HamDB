#include "parser/parser.hpp"
#include "binder/binder.hpp"
#include "planner/planner.hpp"
#include "planner/logical_plan.hpp"
#include "planner/nested_loop_join_plan.hpp"
#include "planner/hash_join_plan.hpp"
#include "catalog/catalog_manager.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include "storage/disk_manager.hpp"
#include <filesystem>
#include <gtest/gtest.h>

namespace hamdb::planner {

class JoinPlanTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        test_db_ = std::filesystem::temp_directory_path() / (std::string("test_join_plan_") + test_name + ".hamdb");
        if (std::filesystem::exists(test_db_)) {
            std::filesystem::remove(test_db_);
        }
        disk_manager_ = std::make_unique<DiskManager>(test_db_.string());
        auto st1 = disk_manager_->createDatabase();
        EXPECT_EQ(st1, Status::Ok);
        auto st2 = disk_manager_->openDatabase();
        EXPECT_EQ(st2, Status::Ok);
        bpm_ = std::make_unique<BufferPoolManager>(10, *disk_manager_);
        catalog_ = std::make_unique<CatalogManager>(bpm_.get());

        std::vector<Column> cols_t1 = {
            Column("id", ColumnType::Integer),
            Column("val_a", ColumnType::Integer)
        };
        Schema schema_t1(cols_t1);
        TableInfo* info1;
        (void)catalog_->createTable("t1", schema_t1, info1);

        std::vector<Column> cols_t2 = {
            Column("id", ColumnType::Integer),
            Column("val_b", ColumnType::Integer)
        };
        Schema schema_t2(cols_t2);
        TableInfo* info2;
        (void)catalog_->createTable("t2", schema_t2, info2);

        binder_ = std::make_unique<binder::Binder>(catalog_.get());
        planner_ = std::make_unique<Planner>(catalog_.get());
    }

    void TearDown() override {
        planner_.reset();
        binder_.reset();
        catalog_.reset();
        bpm_.reset();
        disk_manager_.reset();
        if (std::filesystem::exists(test_db_)) {
            std::filesystem::remove(test_db_);
        }
    }

    std::filesystem::path test_db_;
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> bpm_;
    std::unique_ptr<CatalogManager> catalog_;
    std::unique_ptr<binder::Binder> binder_;
    std::unique_ptr<Planner> planner_;
};

TEST_F(JoinPlanTest, SimpleInnerJoin) {
    std::string sql = "SELECT t1.id, t2.val_b FROM t1 JOIN t2 ON t1.id = t2.id";
    Parser parser(sql);
    auto ast = parser.parseStatement();
    
    auto bound_stmt = binder_->bind(*ast);
    auto logical_plan = planner_->plan(std::move(bound_stmt));

    ASSERT_EQ(logical_plan->getType(), LogicalPlanType::PROJECTION);
    ASSERT_EQ(logical_plan->getChildren().size(), 1);

    auto join_node = logical_plan->getChildren()[0].get();
    ASSERT_EQ(join_node->getType(), LogicalPlanType::HASH_JOIN);

    auto nlj = static_cast<LogicalHashJoinNode*>(join_node);
    ASSERT_NE(nlj->getLeftKeyExpr(), nullptr);
    ASSERT_NE(nlj->getRightKeyExpr(), nullptr);

    ASSERT_EQ(nlj->getChildren().size(), 2);
    auto left = nlj->getChildren()[0].get();
    auto right = nlj->getChildren()[1].get();

    ASSERT_EQ(left->getType(), LogicalPlanType::SEQ_SCAN);
    ASSERT_EQ(right->getType(), LogicalPlanType::SEQ_SCAN);

    auto left_scan = static_cast<SeqScanPlanNode*>(left);
    auto right_scan = static_cast<SeqScanPlanNode*>(right);

    EXPECT_EQ(left_scan->getTableName(), "t1");
    EXPECT_EQ(right_scan->getTableName(), "t2");
}

} // namespace hamdb::planner
