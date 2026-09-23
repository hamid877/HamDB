add_library(hamdb_index STATIC
    btree_page.cpp
    btree_leaf_page.cpp
    btree_internal_page.cpp
    bplus_tree.cpp
    bplus_tree_iterator.cpp
)

target_link_libraries(hamdb_index PUBLIC
    hamdb_common
    hamdb_utils
    hamdb_storage
    hamdb_buffer
)

target_include_directories(hamdb_index PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(hamdb_index PUBLIC cxx_std_20)
