add_executable(planner_test planner_test.cpp)
target_include_directories(planner_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(planner_test PRIVATE GTest::gtest_main hamdb_planner hamdb_binder)
add_test(NAME PlannerTest COMMAND planner_test)

add_executable(physical_planner_test physical_planner_test.cpp)
target_include_directories(physical_planner_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(physical_planner_test PRIVATE GTest::gtest_main hamdb_planner hamdb_binder)
add_test(NAME PhysicalPlannerTest COMMAND physical_planner_test)

add_executable(executor_factory_test executor_factory_test.cpp)
target_include_directories(executor_factory_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(executor_factory_test PRIVATE GTest::gtest_main hamdb_planner hamdb_binder hamdb_executor hamdb_transaction hamdb_wal hamdb_parser)
add_test(NAME ExecutorFactoryTest COMMAND executor_factory_test)

add_executable(explain_test explain_test.cpp)
target_include_directories(explain_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(explain_test PRIVATE GTest::gtest_main hamdb_shell hamdb_planner hamdb_binder hamdb_executor hamdb_transaction hamdb_wal hamdb_parser)
add_test(NAME ExplainTest COMMAND explain_test)

add_executable(analyze_test analyze_test.cpp)
target_include_directories(analyze_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(analyze_test PRIVATE GTest::gtest_main hamdb_shell hamdb_planner hamdb_binder hamdb_executor hamdb_transaction hamdb_wal hamdb_parser)
add_test(NAME AnalyzeTest COMMAND analyze_test)

add_executable(join_plan_test join_plan_test.cpp)
target_include_directories(join_plan_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(join_plan_test PRIVATE GTest::gtest_main hamdb_planner hamdb_binder hamdb_parser)
add_test(NAME JoinPlanTest COMMAND join_plan_test)

add_executable(hash_join_plan_test hash_join_plan_test.cpp)
target_include_directories(hash_join_plan_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(hash_join_plan_test PRIVATE GTest::gtest_main hamdb_planner hamdb_binder hamdb_parser)
add_test(NAME HashJoinPlanTest COMMAND hash_join_plan_test)

add_executable(aggregation_plan_test aggregation_plan_test.cpp)
target_include_directories(aggregation_plan_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(aggregation_plan_test PRIVATE GTest::gtest_main hamdb_planner hamdb_binder hamdb_parser)
add_test(NAME AggregationPlanTest COMMAND aggregation_plan_test)
