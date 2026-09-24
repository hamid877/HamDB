add_library(hamdb_executor STATIC
    value.cpp
    column_value_expression.cpp
    executor_context.cpp
    seq_scan_executor.cpp
)

target_link_libraries(hamdb_executor PUBLIC
    hamdb_common
    hamdb_storage
    hamdb_utils
    hamdb_catalog
    hamdb_transaction
    hamdb_buffer
)

target_include_directories(hamdb_executor PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
