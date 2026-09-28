add_executable(shell_test
    shell_test.cpp
    table_printer_test.cpp
    script_executor_test.cpp
)

target_link_libraries(shell_test PRIVATE
    hamdb_core
    hamdb_shell
    gtest_main
)

target_include_directories(shell_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_test(NAME ShellTest COMMAND shell_test)
