add_library(hamdb_server
    server.cpp
    session.cpp
)

target_link_libraries(hamdb_server PUBLIC
    hamdb_shell
)

target_include_directories(hamdb_server PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
