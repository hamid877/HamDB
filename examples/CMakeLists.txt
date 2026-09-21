add_executable(hamdb_example_create_database
    create_database.cpp
)

target_link_libraries(hamdb_example_create_database PRIVATE
    hamdb_core
)

target_compile_features(hamdb_example_create_database PRIVATE cxx_std_20)
