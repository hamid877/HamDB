add_executable(hamdb
    hamdb.cpp
)

target_link_libraries(hamdb PRIVATE
    hamdb_core
    hamdb_shell
)

target_include_directories(hamdb PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
