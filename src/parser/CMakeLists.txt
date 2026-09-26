add_library(hamdb_parser STATIC
    token.cpp
    lexer.cpp
    parser.cpp
    ast/expression.cpp
    ast/statement.cpp
)

target_include_directories(hamdb_parser PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(hamdb_parser PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)
