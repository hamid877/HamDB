add_executable(catalog_test
    catalog_test.cpp
)

target_link_libraries(catalog_test PRIVATE
    hamdb_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(catalog_test)

add_executable(statistics_test
    statistics_test.cpp
)

target_link_libraries(statistics_test PRIVATE
    hamdb_catalog
    hamdb_storage
    hamdb_executor
    hamdb_buffer
    GTest::gtest_main
)
target_include_directories(statistics_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
gtest_discover_tests(statistics_test)

