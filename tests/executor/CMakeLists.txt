add_executable(hamdb_executor_test
    expression_test.cpp
    seq_scan_executor_test.cpp
    index_scan_executor_test.cpp
    insert_executor_test.cpp
    delete_executor_test.cpp
    update_executor_test.cpp
    filter_executor_test.cpp
    projection_executor_test.cpp
    nested_loop_join_executor_test.cpp
    aggregation_executor_test.cpp
)

target_link_libraries(hamdb_executor_test PRIVATE
    gtest_main
    hamdb_core
)

add_test(NAME ExecutorTest COMMAND hamdb_executor_test)
