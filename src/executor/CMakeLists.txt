add_library(hamdb_executor STATIC
    value.cpp
    column_value_expression.cpp
    executor_context.cpp
    seq_scan_executor.cpp
    index_scan_executor.cpp
    insert_executor.cpp
    delete_executor.cpp
    update_executor.cpp
    filter_executor.cpp
    projection_executor.cpp
    nested_loop_join_executor.cpp
    aggregation_hash_table.cpp
    aggregation_executor.cpp
    sort_executor.cpp
    limit_executor.cpp
    values_executor.cpp
)

target_link_libraries(hamdb_executor PUBLIC
    hamdb_common
    hamdb_storage
    hamdb_utils
    hamdb_catalog
    hamdb_transaction
    hamdb_buffer
    hamdb_index
    hamdb_wal
)

target_include_directories(hamdb_executor PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
