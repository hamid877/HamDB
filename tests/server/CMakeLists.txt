add_executable(server_test
    server_test.cpp
)

target_link_libraries(server_test PRIVATE
    GTest::gtest_main
    hamdb_core
)

target_include_directories(server_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_test(NAME server_test COMMAND server_test)
