#include <gtest/gtest.h>
#include "shell/script_executor.hpp"
#include <string>
#include <vector>

using namespace hamdb::shell;

TEST(StatementSplitterTest, BasicSplit) {
    std::string script = "SELECT 1; SELECT 2; SELECT 3;";
    auto stmts = StatementSplitter::split(script);
    ASSERT_EQ(stmts.size(), 3);
    EXPECT_EQ(stmts[0], "SELECT 1;");
    EXPECT_EQ(stmts[1], " SELECT 2;");
    EXPECT_EQ(stmts[2], " SELECT 3;");
}

TEST(StatementSplitterTest, Strings) {
    std::string script = "SELECT 'hello;world'; SELECT 2;";
    auto stmts = StatementSplitter::split(script);
    ASSERT_EQ(stmts.size(), 2);
    EXPECT_EQ(stmts[0], "SELECT 'hello;world';");
    EXPECT_EQ(stmts[1], " SELECT 2;");
}

TEST(StatementSplitterTest, LineComments) {
    std::string script = "SELECT 1; -- this is a comment;\nSELECT 2;";
    auto stmts = StatementSplitter::split(script);
    ASSERT_EQ(stmts.size(), 2);
    EXPECT_EQ(stmts[0], "SELECT 1;");
    EXPECT_EQ(stmts[1], " -- this is a comment;\nSELECT 2;");
}

TEST(StatementSplitterTest, BlockComments) {
    std::string script = "SELECT 1; /* \nblock comment;\n */ SELECT 2;";
    auto stmts = StatementSplitter::split(script);
    ASSERT_EQ(stmts.size(), 2);
    EXPECT_EQ(stmts[0], "SELECT 1;");
    EXPECT_EQ(stmts[1], " /* \nblock comment;\n */ SELECT 2;");
}

TEST(StatementSplitterTest, TrailingNoSemicolon) {
    std::string script = "SELECT 1; SELECT 2";
    auto stmts = StatementSplitter::split(script);
    ASSERT_EQ(stmts.size(), 2);
    EXPECT_EQ(stmts[0], "SELECT 1;");
    EXPECT_EQ(stmts[1], " SELECT 2");
}

TEST(StatementSplitterTest, EmptyStatements) {
    std::string script = "SELECT 1; ;  ; SELECT 2;";
    auto stmts = StatementSplitter::split(script);
    // Our logic currently filters out empty statements containing only spaces
    ASSERT_EQ(stmts.size(), 2);
    EXPECT_EQ(stmts[0], "SELECT 1;");
    EXPECT_EQ(stmts[1], " SELECT 2;");
}
