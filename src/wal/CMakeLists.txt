add_library(hamdb_wal STATIC
    log_record.cpp
    log_manager.cpp
    recovery_manager.cpp
)

target_include_directories(hamdb_wal PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(hamdb_wal PUBLIC
    hamdb_common
    hamdb_storage
    hamdb_transaction
    hamdb_utils
)
