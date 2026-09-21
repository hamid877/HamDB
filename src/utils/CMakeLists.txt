add_library(hamdb_utils STATIC
    serializer.cpp
    deserializer.cpp
)

target_link_libraries(hamdb_utils PUBLIC hamdb_common)

target_include_directories(hamdb_utils PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(hamdb_utils PUBLIC cxx_std_20)
