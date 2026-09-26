add_executable(binder_test binder_test.cpp)

target_link_libraries(binder_test PRIVATE
    hamdb_core
    GTest::gtest_main
)

add_test(NAME BinderTest COMMAND binder_test)
