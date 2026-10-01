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

add_executable(tcp_transport_test
    tcp_transport_test.cpp
)

target_link_libraries(tcp_transport_test PRIVATE
    GTest::gtest_main
    hamdb_core
)

target_include_directories(tcp_transport_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_test(NAME tcp_transport_test COMMAND tcp_transport_test)

add_executable(wire_protocol_test
    wire_protocol_test.cpp
)

target_link_libraries(wire_protocol_test PRIVATE
    GTest::gtest_main
    hamdb_core
)

target_include_directories(wire_protocol_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_test(NAME wire_protocol_test COMMAND wire_protocol_test)

add_executable(query_execution_test
    query_execution_test.cpp
)

target_link_libraries(query_execution_test PRIVATE
    GTest::gtest_main
    hamdb_core
)

target_include_directories(query_execution_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_test(NAME query_execution_test COMMAND query_execution_test)
