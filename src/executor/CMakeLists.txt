add_library(hamdb_executor STATIC
    value.cpp
    column_value_expression.cpp
)

target_link_libraries(hamdb_executor PUBLIC
    hamdb_common
    hamdb_storage
    hamdb_utils
    hamdb_catalog
)

target_include_directories(hamdb_executor PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
