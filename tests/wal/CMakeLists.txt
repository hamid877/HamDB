add_executable(wal_test wal_test.cpp)

target_link_libraries(wal_test PRIVATE
    hamdb_core
    GTest::gtest_main
)

add_test(NAME WalTest COMMAND wal_test)
