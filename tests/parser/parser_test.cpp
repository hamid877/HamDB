#include <gtest/gtest.h>
#include "parser/parser.hpp"

using namespace hamdb;

TEST(ParserTest, SelectSimple) {
    Parser parser("SELECT id, name FROM users");
    auto stmt = parser.parseStatement();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->toString(), "SELECT id, name FROM users");
}

TEST(ParserTest, SelectWhereOrderLimitOffset) {
    Parser parser("SELECT id FROM users WHERE age >= 18 ORDER BY age DESC, name ASC LIMIT 10 OFFSET 5");
    auto stmt = parser.parseStatement();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->toString(), "SELECT id FROM users WHERE (age >= 18) ORDER BY age DESC, name ASC LIMIT 10 OFFSET 5");
}

TEST(ParserTest, InsertStatement) {
    Parser parser("INSERT INTO users VALUES (1, 'Alice'), (2, 'Bob')");
    auto stmt = parser.parseStatement();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->toString(), "INSERT INTO users VALUES (1, 'Alice'), (2, 'Bob')");
}

TEST(ParserTest, UpdateStatement) {
    Parser parser("UPDATE users SET age = 20, name = 'Charlie' WHERE id = 1");
    auto stmt = parser.parseStatement();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->toString(), "UPDATE users SET age = 20, name = 'Charlie' WHERE (id = 1)");
}

TEST(ParserTest, DeleteStatement) {
    Parser parser("DELETE FROM users WHERE id = 1");
    auto stmt = parser.parseStatement();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->toString(), "DELETE FROM users WHERE (id = 1)");
}

TEST(ParserTest, ValuesStatement) {
    Parser parser("VALUES (1, 2), (3, 4)");
    auto stmt = parser.parseStatement();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->toString(), "VALUES (1, 2), (3, 4)");
}

TEST(ParserTest, Expressions) {
    Parser parser("1 + 2 * 3 - 4 / 5 % 6");
    auto expr = parser.parseExpression();
    ASSERT_NE(expr, nullptr);
    // (1 + (2 * 3)) - ((4 / 5) % 6)
    EXPECT_EQ(expr->toString(), "((1 + (2 * 3)) - ((4 / 5) % 6))");
}

TEST(ParserTest, ExpressionsParenthesesAndUnary) {
    Parser parser("-(1 + 2) * +3");
    auto expr = parser.parseExpression();
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(expr->toString(), "((-(1 + 2)) * (+3))");
}

TEST(ParserTest, LogicAndComparison) {
    Parser parser("a = b AND c != d OR e < f AND g >= h");
    auto expr = parser.parseExpression();
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(expr->toString(), "(((a = b) AND (c != d)) OR ((e < f) AND (g >= h)))");
}

TEST(ParserTest, ParseError) {
    Parser parser("SELECT FROM");
    EXPECT_THROW(parser.parseStatement(), ParserError);
}
