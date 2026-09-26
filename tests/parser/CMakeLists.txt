add_executable(hamdb_parser_test
    lexer_test.cpp
    parser_test.cpp
)

target_link_libraries(hamdb_parser_test PRIVATE
    hamdb_parser
    GTest::gtest_main
)

add_test(NAME hamdb_parser_test COMMAND hamdb_parser_test)
