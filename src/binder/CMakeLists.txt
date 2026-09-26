add_library(hamdb_binder STATIC
    binder.cpp
)

target_include_directories(hamdb_binder PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(hamdb_binder PUBLIC
    hamdb_parser
    hamdb_catalog
    hamdb_executor
)

target_compile_options(hamdb_binder PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)
