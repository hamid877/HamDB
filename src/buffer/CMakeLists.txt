add_library(hamdb_buffer STATIC
    buffer_frame.cpp
    buffer_pool_manager.cpp
)

target_link_libraries(hamdb_buffer PUBLIC
    hamdb_storage
    hamdb_common
)

target_include_directories(hamdb_buffer PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(hamdb_buffer PUBLIC cxx_std_20)
