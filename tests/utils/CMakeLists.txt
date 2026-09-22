add_executable(hamdb_utils_tests
    serializer_test.cpp
)

target_link_libraries(hamdb_utils_tests PRIVATE
    hamdb_utils
    hamdb_common
    GTest::gtest_main
)

target_include_directories(hamdb_utils_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(hamdb_utils_tests PRIVATE cxx_std_20)

include(GoogleTest)
gtest_discover_tests(hamdb_utils_tests)
