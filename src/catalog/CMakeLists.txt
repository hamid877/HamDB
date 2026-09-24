add_library(hamdb_catalog STATIC
    schema.cpp
    table_info.cpp
    catalog_manager.cpp
)

target_link_libraries(hamdb_catalog PUBLIC
    hamdb_common
    hamdb_utils
    hamdb_storage
    hamdb_buffer
)

target_include_directories(hamdb_catalog PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(hamdb_catalog PUBLIC cxx_std_20)
