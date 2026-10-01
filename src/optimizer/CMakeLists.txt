add_library(hamdb_optimizer
    optimizer.cpp
    predicate_pushdown_rule.cpp
    projection_pruning_rule.cpp
    constant_folding_rule.cpp
    index_scan_rule.cpp
    sort_limit_rule.cpp
    join_selection_rule.cpp
    top_k_optimization_rule.cpp
    rule_executor.cpp
    cardinality_estimator.cpp
    cost_model.cpp
    cost_based_join_rule.cpp
)
target_include_directories(hamdb_optimizer PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(hamdb_optimizer PUBLIC hamdb_planner hamdb_executor)
