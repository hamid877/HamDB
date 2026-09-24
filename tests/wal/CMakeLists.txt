add_executable(wal_test wal_test.cpp)

target_link_libraries(wal_test PRIVATE
    hamdb_core
    GTest::gtest_main
)

add_test(NAME WalTest COMMAND wal_test)

add_executable(recovery_manager_test recovery_manager_test.cpp)

target_link_libraries(recovery_manager_test PRIVATE
    hamdb_core
    GTest::gtest_main
)

add_test(NAME RecoveryManagerTest COMMAND recovery_manager_test)
