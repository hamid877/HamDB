add_library(hamdb_optimizer
    predicate_pushdown_rule.cpp
    rule_executor.cpp
)
target_include_directories(hamdb_optimizer PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(hamdb_optimizer PUBLIC hamdb_planner hamdb_executor)
