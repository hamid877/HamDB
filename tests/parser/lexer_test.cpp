#include <gtest/gtest.h>
#include "parser/lexer.hpp"
#include "parser/token.hpp"
#include <vector>

using namespace hamdb;

TEST(LexerTest, EmptySource) {
    Lexer lexer("");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::Eof);
}

TEST(LexerTest, KeywordsAndIdentifiers) {
    Lexer lexer("SELECT FROM table_name WHERE id = 1;");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens.size(), 9);
    EXPECT_EQ(tokens[0].type, TokenType::Select);
    EXPECT_EQ(tokens[1].type, TokenType::From);
    EXPECT_EQ(tokens[2].type, TokenType::Identifier);
    EXPECT_EQ(tokens[2].lexeme, "table_name");
    EXPECT_EQ(tokens[3].type, TokenType::Where);
    EXPECT_EQ(tokens[4].type, TokenType::Identifier);
    EXPECT_EQ(tokens[4].lexeme, "id");
    EXPECT_EQ(tokens[5].type, TokenType::Equals);
    EXPECT_EQ(tokens[6].type, TokenType::Integer);
    EXPECT_EQ(tokens[6].lexeme, "1");
    EXPECT_EQ(tokens[7].type, TokenType::Semicolon);
    EXPECT_EQ(tokens[8].type, TokenType::Eof);
}

TEST(LexerTest, CaseInsensitiveKeywords) {
    Lexer lexer("SeLeCt fRoM");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type, TokenType::Select);
    EXPECT_EQ(tokens[1].type, TokenType::From);
    EXPECT_EQ(tokens[2].type, TokenType::Eof);
}

TEST(LexerTest, StringsAndEscapes) {
    Lexer lexer("'hello world' 'hello ''world'''");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type, TokenType::String);
    EXPECT_EQ(tokens[0].lexeme, "hello world");
    EXPECT_EQ(tokens[1].type, TokenType::String);
    EXPECT_EQ(tokens[1].lexeme, "hello 'world'");
    EXPECT_EQ(tokens[2].type, TokenType::Eof);
}

TEST(LexerTest, UnterminatedString) {
    Lexer lexer("'unfinished");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].type, TokenType::Invalid);
    EXPECT_EQ(tokens[0].lexeme, "Unterminated string");
    EXPECT_EQ(tokens[1].type, TokenType::Eof);
}

TEST(LexerTest, OperatorsAndDelimiters) {
    Lexer lexer("+, - *, /,= != < <= > >= ( ) ; .");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens.size(), 18);
    EXPECT_EQ(tokens[0].type, TokenType::Plus);
    EXPECT_EQ(tokens[1].type, TokenType::Comma);
    EXPECT_EQ(tokens[2].type, TokenType::Minus);
    EXPECT_EQ(tokens[3].type, TokenType::Asterisk);
    EXPECT_EQ(tokens[4].type, TokenType::Comma);
    EXPECT_EQ(tokens[5].type, TokenType::Slash);
    EXPECT_EQ(tokens[6].type, TokenType::Comma);
    EXPECT_EQ(tokens[7].type, TokenType::Equals);
    EXPECT_EQ(tokens[8].type, TokenType::NotEquals);
    EXPECT_EQ(tokens[9].type, TokenType::Less);
    EXPECT_EQ(tokens[10].type, TokenType::LessEquals);
    EXPECT_EQ(tokens[11].type, TokenType::Greater);
    EXPECT_EQ(tokens[12].type, TokenType::GreaterEquals);
    EXPECT_EQ(tokens[13].type, TokenType::LeftParen);
    EXPECT_EQ(tokens[14].type, TokenType::RightParen);
    EXPECT_EQ(tokens[15].type, TokenType::Semicolon);
    EXPECT_EQ(tokens[16].type, TokenType::Dot);
    EXPECT_EQ(tokens[17].type, TokenType::Eof);
}

TEST(LexerTest, OperatorsAndDelimitersCorrect) {
    Lexer lexer("+ - * / = != < <= > >= ( ) ; . ,");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::Plus);
    EXPECT_EQ(tokens[1].type, TokenType::Minus);
    EXPECT_EQ(tokens[2].type, TokenType::Asterisk);
    EXPECT_EQ(tokens[3].type, TokenType::Slash);
    EXPECT_EQ(tokens[4].type, TokenType::Equals);
    EXPECT_EQ(tokens[5].type, TokenType::NotEquals);
    EXPECT_EQ(tokens[6].type, TokenType::Less);
    EXPECT_EQ(tokens[7].type, TokenType::LessEquals);
    EXPECT_EQ(tokens[8].type, TokenType::Greater);
    EXPECT_EQ(tokens[9].type, TokenType::GreaterEquals);
    EXPECT_EQ(tokens[10].type, TokenType::LeftParen);
    EXPECT_EQ(tokens[11].type, TokenType::RightParen);
    EXPECT_EQ(tokens[12].type, TokenType::Semicolon);
    EXPECT_EQ(tokens[13].type, TokenType::Dot);
    EXPECT_EQ(tokens[14].type, TokenType::Comma);
    EXPECT_EQ(tokens[15].type, TokenType::Eof);
}

TEST(LexerTest, Comments) {
    Lexer lexer("SELECT -- this is a comment\n1 /* multi\nline */ FROM");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens.size(), 4);
    EXPECT_EQ(tokens[0].type, TokenType::Select);
    EXPECT_EQ(tokens[1].type, TokenType::Integer);
    EXPECT_EQ(tokens[1].lexeme, "1");
    EXPECT_EQ(tokens[2].type, TokenType::From);
    EXPECT_EQ(tokens[3].type, TokenType::Eof);
}

TEST(LexerTest, LineAndColumnPositions) {
    Lexer lexer("SELECT\n  id\nFROM");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens.size(), 4);
    EXPECT_EQ(tokens[0].line, 1);
    EXPECT_EQ(tokens[0].column, 1);
    EXPECT_EQ(tokens[1].line, 2);
    EXPECT_EQ(tokens[1].column, 3);
    EXPECT_EQ(tokens[2].line, 3);
    EXPECT_EQ(tokens[2].column, 1);
}

TEST(LexerTest, BooleanAndNullLiterals) {
    Lexer lexer("TRUE false NuLl");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens.size(), 4);
    EXPECT_EQ(tokens[0].type, TokenType::BoolLiteral);
    EXPECT_EQ(tokens[0].lexeme, "TRUE");
    EXPECT_EQ(tokens[1].type, TokenType::BoolLiteral);
    EXPECT_EQ(tokens[1].lexeme, "false");
    EXPECT_EQ(tokens[2].type, TokenType::NullLiteral);
    EXPECT_EQ(tokens[2].lexeme, "NuLl");
}

TEST(LexerTest, InvalidCharacter) {
    Lexer lexer("SELECT @;");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens.size(), 4);
    EXPECT_EQ(tokens[0].type, TokenType::Select);
    EXPECT_EQ(tokens[1].type, TokenType::Invalid);
    EXPECT_EQ(tokens[1].line, 1);
    EXPECT_EQ(tokens[1].column, 8);
    EXPECT_EQ(tokens[2].type, TokenType::Semicolon);
    EXPECT_EQ(tokens[3].type, TokenType::Eof);
}
