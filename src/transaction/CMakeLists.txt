add_library(hamdb_transaction STATIC
    transaction.cpp
    transaction_manager.cpp
    lock_manager.cpp
)

target_include_directories(hamdb_transaction PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(hamdb_transaction PUBLIC
    hamdb_common
)
