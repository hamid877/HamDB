add_library(hamdb_common STATIC
    types.cpp
    serializer.cpp
    deserializer.cpp
)

target_include_directories(hamdb_common PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(hamdb_common PUBLIC cxx_std_20)
