add_executable(hamdb_buffer_tests
    buffer_pool_manager_test.cpp
    lruk_replacer_test.cpp
)

target_link_libraries(hamdb_buffer_tests PRIVATE
    hamdb_buffer
    hamdb_common
    GTest::gtest_main
)

target_include_directories(hamdb_buffer_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(hamdb_buffer_tests PRIVATE cxx_std_20)

include(GoogleTest)
gtest_discover_tests(hamdb_buffer_tests)
