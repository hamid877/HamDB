set(PLANNER_SOURCES
    planner.cpp
    physical_planner.cpp
    executor_factory.cpp
)

add_library(hamdb_planner STATIC ${PLANNER_SOURCES})
target_include_directories(hamdb_planner PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(hamdb_planner PUBLIC hamdb_binder hamdb_catalog hamdb_executor)
