add_library(hamdb_database STATIC
    database.cpp
)

target_link_libraries(hamdb_database PUBLIC
    hamdb_common
    hamdb_storage
)

target_include_directories(hamdb_database PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(hamdb_database PUBLIC cxx_std_20)
