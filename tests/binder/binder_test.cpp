#include "binder/binder.hpp"
#include "parser/parser.hpp"
#include "parser/lexer.hpp"
#include "catalog/catalog_manager.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include "storage/disk_manager.hpp"
#include <gtest/gtest.h>

namespace hamdb {

class BinderTest : public ::testing::Test {
protected:
    void SetUp() override {
        disk_manager_ = std::make_unique<DiskManager>("binder_test.db");
        auto st = disk_manager_->createDatabase();
        ASSERT_EQ(st, Status::Ok);
        st = disk_manager_->openDatabase();
        ASSERT_EQ(st, Status::Ok);
        bpm_ = std::make_unique<BufferPoolManager>(10, *disk_manager_);
        catalog_ = std::make_unique<CatalogManager>(bpm_.get());

        // Create a test table
        std::vector<Column> cols = {
            Column("id", ColumnType::Integer),
            Column("name", ColumnType::Varchar),
            Column("active", ColumnType::Boolean)
        };
        Schema schema(cols);
        TableInfo* out_info = nullptr;
        auto status = catalog_->createTable("users", schema, out_info);
        ASSERT_EQ(status, Status::Ok);
    }

    void TearDown() override {
        remove("binder_test.db");
    }

    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> bpm_;
    std::unique_ptr<CatalogManager> catalog_;
};

TEST_F(BinderTest, BindSelectAll) {
    Parser parser("SELECT * FROM users");
    auto ast = parser.parseStatement();
    
    binder::Binder binder(catalog_.get());
    auto bound_stmt = binder.bind(*ast);
    ASSERT_EQ(bound_stmt->getType(), binder::BoundStatementType::SELECT);
    
    auto* sel = dynamic_cast<binder::BoundSelectStatement*>(bound_stmt.get());
    ASSERT_NE(sel, nullptr);
    ASSERT_EQ(sel->table_name_, "users");
    
    // '*' expanded to 3 columns
    ASSERT_EQ(sel->select_list_.size(), 3);
    EXPECT_EQ(sel->select_list_[0]->getType(), TypeId::Integer);
    EXPECT_EQ(sel->select_list_[1]->getType(), TypeId::Varchar);
    EXPECT_EQ(sel->select_list_[2]->getType(), TypeId::Boolean);
}

TEST_F(BinderTest, BindSelectWithAlias) {
    Parser parser("SELECT id FROM users u");
    auto ast = parser.parseStatement();
    
    binder::Binder binder(catalog_.get());
    auto bound_stmt = binder.bind(*ast);
    auto* sel = dynamic_cast<binder::BoundSelectStatement*>(bound_stmt.get());
    
    ASSERT_EQ(sel->table_name_, "users");
    ASSERT_EQ(sel->table_alias_, "u");
    ASSERT_EQ(sel->select_list_.size(), 1);
    
    auto col_ref = dynamic_cast<binder::BoundColumnRef*>(sel->select_list_[0].get());
    ASSERT_NE(col_ref, nullptr);
    EXPECT_EQ(col_ref->getColumnName(), "id");
}

TEST_F(BinderTest, UnknownTable) {
    Parser parser("SELECT * FROM foo");
    auto ast = parser.parseStatement();
    
    binder::Binder binder(catalog_.get());
    EXPECT_THROW(binder.bind(*ast), binder::BinderError);
}

TEST_F(BinderTest, UnknownColumn) {
    Parser parser("SELECT foo FROM users");
    auto ast = parser.parseStatement();
    
    binder::Binder binder(catalog_.get());
    EXPECT_THROW(binder.bind(*ast), binder::BinderError);
}

TEST_F(BinderTest, TypeMismatchInWhere) {
    // WHERE id (which is int)
    Parser parser("SELECT * FROM users WHERE id");
    auto ast = parser.parseStatement();
    
    binder::Binder binder(catalog_.get());
    EXPECT_THROW(binder.bind(*ast), binder::BinderError);
}

TEST_F(BinderTest, BindInsertTypeMismatch) {
    // Inserting boolean into Integer column
    Parser parser("INSERT INTO users VALUES (true, 'alice', false)");
    auto ast = parser.parseStatement();
    
    binder::Binder binder(catalog_.get());
    EXPECT_THROW(binder.bind(*ast), binder::BinderError);
}

TEST_F(BinderTest, BindUpdate) {
    Parser parser("UPDATE users SET name = 'alice' WHERE active = true");
    auto ast = parser.parseStatement();
    
    binder::Binder binder(catalog_.get());
    auto bound_stmt = binder.bind(*ast);
    
    auto* upd = dynamic_cast<binder::BoundUpdateStatement*>(bound_stmt.get());
    ASSERT_NE(upd, nullptr);
    ASSERT_EQ(upd->set_clauses_.size(), 1);
    EXPECT_EQ(upd->set_clauses_[0].first, "name");
    EXPECT_EQ(upd->set_clauses_[0].second->getType(), TypeId::Varchar);
}

} // namespace hamdb
