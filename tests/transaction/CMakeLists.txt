add_executable(transaction_test transaction_test.cpp)

target_link_libraries(transaction_test PRIVATE
    hamdb_core
    GTest::gtest_main
)

add_test(NAME TransactionTest COMMAND transaction_test)
