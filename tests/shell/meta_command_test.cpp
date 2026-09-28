#include <gtest/gtest.h>
#include <filesystem>
#include <sstream>
#include "shell/shell.hpp"

namespace hamdb::shell::test {

class MetaCommandTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        db_path_ = std::filesystem::temp_directory_path() / (std::string("test_meta_") + test_name + ".db");
        if (std::filesystem::exists(db_path_)) {
            std::filesystem::remove(db_path_);
        }
        shell_ = std::make_unique<Shell>(db_path_.string());
        
        // Setup some tables for testing
        std::vector<Column> cols = {
            Column("id", ColumnType::Integer),
            Column("name", ColumnType::Varchar)
        };
        Schema schema(cols);
        TableInfo* info = nullptr;
        (void)shell_->getCatalog()->createTable("users", schema, info);
    }

    void TearDown() override {
        shell_.reset();
        if (std::filesystem::exists(db_path_)) {
            std::filesystem::remove(db_path_);
        }
    }

    std::filesystem::path db_path_;
    std::unique_ptr<Shell> shell_;
};

TEST_F(MetaCommandTest, HelpCommand) {
    std::stringstream out;
    shell_->executeMeta(".help", out);
    EXPECT_TRUE(out.str().find(".tables") != std::string::npos);
    EXPECT_TRUE(out.str().find(".schema") != std::string::npos);
}

TEST_F(MetaCommandTest, TablesCommand) {
    std::stringstream out;
    shell_->executeMeta(".tables", out);
    EXPECT_TRUE(out.str().find("users") != std::string::npos);
}

TEST_F(MetaCommandTest, SchemaCommand) {
    std::stringstream out;
    shell_->executeMeta(".schema users", out);
    EXPECT_TRUE(out.str().find("CREATE TABLE users") != std::string::npos);
    EXPECT_TRUE(out.str().find("id INTEGER") != std::string::npos);
    EXPECT_TRUE(out.str().find("name VARCHAR") != std::string::npos);
}

TEST_F(MetaCommandTest, UnknownSchemaCommand) {
    std::stringstream out;
    shell_->executeMeta(".schema unknown_table", out);
    EXPECT_TRUE(out.str().find("Error:") != std::string::npos);
}

TEST_F(MetaCommandTest, DescribeCommand) {
    std::stringstream out;
    shell_->executeMeta(".describe users", out);
    EXPECT_TRUE(out.str().find("id") != std::string::npos);
    EXPECT_TRUE(out.str().find("INTEGER") != std::string::npos);
    EXPECT_TRUE(out.str().find("name") != std::string::npos);
    EXPECT_TRUE(out.str().find("VARCHAR") != std::string::npos);
}

TEST_F(MetaCommandTest, IndexesCommand) {
    std::stringstream out;
    shell_->executeMeta(".indexes users", out);
    EXPECT_TRUE(out.str().find("Index on users") != std::string::npos);
}

TEST_F(MetaCommandTest, StatsCommand) {
    std::stringstream out;
    shell_->executeMeta(".stats", out);
    EXPECT_TRUE(out.str().find("Buffer Pool Size") != std::string::npos);
    EXPECT_TRUE(out.str().find("Disk Pages") != std::string::npos);
    EXPECT_TRUE(out.str().find("Active Transactions") != std::string::npos);
}

TEST_F(MetaCommandTest, UnknownCommand) {
    std::stringstream out;
    shell_->executeMeta(".unknown", out);
    EXPECT_TRUE(out.str().find("Error: unknown command") != std::string::npos);
}

} // namespace hamdb::shell::test
