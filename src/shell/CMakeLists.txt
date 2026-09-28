add_library(hamdb_shell STATIC
    table_printer.cpp
    shell.cpp
    meta_commands.cpp
    repl.cpp
    script_executor.cpp
)

target_link_libraries(hamdb_shell PUBLIC
    hamdb_common
    hamdb_storage
    hamdb_utils
    hamdb_catalog
    hamdb_transaction
    hamdb_buffer
    hamdb_index
    hamdb_wal
    hamdb_parser
    hamdb_binder
    hamdb_planner
    hamdb_executor
    hamdb_optimizer
    hamdb_prepared
)

target_include_directories(hamdb_shell PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
