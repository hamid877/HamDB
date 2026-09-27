#include "shell/shell.hpp"
#include <iostream>
int main() {
    hamdb::shell::Shell shell("test_explain.db");
    shell.executeSQL("CREATE TABLE users (id INT, name VARCHAR, age INT);", std::cout);
    shell.executeSQL("INSERT INTO users VALUES (1, 'Alice', 30), (2, 'Bob', 25), (3, 'Charlie', 35);", std::cout);
    shell.executeSQL("EXPLAIN SELECT id, name FROM users WHERE age > 25;", std::cout);
    return 0;
}
