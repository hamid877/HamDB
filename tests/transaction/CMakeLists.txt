add_executable(transaction_test transaction_test.cpp)

target_link_libraries(transaction_test PRIVATE
    hamdb_core
    GTest::gtest_main
)

add_test(NAME TransactionTest COMMAND transaction_test)

add_executable(lock_manager_test lock_manager_test.cpp)
target_link_libraries(lock_manager_test PRIVATE hamdb_core GTest::gtest_main)
add_test(NAME LockManagerTest COMMAND lock_manager_test)
