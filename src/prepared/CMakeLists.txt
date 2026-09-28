add_library(hamdb_prepared
    prepared_statement.cpp
    prepared_statement_manager.cpp
)

target_include_directories(hamdb_prepared PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(hamdb_prepared PUBLIC hamdb_executor hamdb_planner hamdb_binder)
