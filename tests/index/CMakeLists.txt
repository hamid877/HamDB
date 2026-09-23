add_executable(hamdb_index_tests
    btree_page_test.cpp
    btree_leaf_page_test.cpp
    btree_internal_page_test.cpp
    bplus_tree_test.cpp
)

target_link_libraries(hamdb_index_tests PRIVATE
    hamdb_index
    hamdb_common
    hamdb_storage
    hamdb_buffer
    GTest::gtest_main
)

target_include_directories(hamdb_index_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(hamdb_index_tests PRIVATE cxx_std_20)

include(GoogleTest)
gtest_discover_tests(hamdb_index_tests)
