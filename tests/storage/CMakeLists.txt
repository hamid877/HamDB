add_executable(hamdb_storage_tests
    page_header_test.cpp
    page_test.cpp
    disk_manager_test.cpp
    slotted_page_test.cpp
)

target_link_libraries(hamdb_storage_tests PRIVATE
    hamdb_storage
    hamdb_common
    GTest::gtest_main
)

target_include_directories(hamdb_storage_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(hamdb_storage_tests PRIVATE cxx_std_20)

include(GoogleTest)
gtest_discover_tests(hamdb_storage_tests)
