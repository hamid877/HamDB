add_executable(planner_test planner_test.cpp)
target_include_directories(planner_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(planner_test PRIVATE GTest::gtest_main hamdb_planner hamdb_binder)
add_test(NAME PlannerTest COMMAND planner_test)
