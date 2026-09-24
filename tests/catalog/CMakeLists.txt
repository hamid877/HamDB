add_executable(catalog_test
    catalog_test.cpp
)

target_link_libraries(catalog_test PRIVATE
    hamdb_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(catalog_test)
