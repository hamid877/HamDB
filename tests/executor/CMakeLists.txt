add_executable(hamdb_executor_test
    expression_test.cpp
)

target_link_libraries(hamdb_executor_test PRIVATE
    gtest_main
    hamdb_core
)

add_test(NAME ExecutorTest COMMAND hamdb_executor_test)
