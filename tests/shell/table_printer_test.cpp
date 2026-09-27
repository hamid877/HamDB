#include "shell/table_printer.hpp"
#include <gtest/gtest.h>
#include <sstream>

namespace hamdb::shell {

TEST(TablePrinterTest, PrintEmpty) {
    TablePrinter printer;
    std::ostringstream os;
    printer.print(os);
    EXPECT_EQ(os.str(), "");
}

TEST(TablePrinterTest, PrintRows) {
    TablePrinter printer;
    printer.setSchema({"id", "name"});
    printer.addRow({"1", "Alice"});
    printer.addRow({"2", "Bob"});
    std::ostringstream os;
    printer.print(os);
    
    std::string expected = 
        "+----+-------+\n"
        "| id | name  |\n"
        "+----+-------+\n"
        "| 1  | Alice |\n"
        "| 2  | Bob   |\n"
        "+----+-------+\n";
    EXPECT_EQ(os.str(), expected);
}

TEST(TablePrinterTest, PrintRowCount) {
    TablePrinter printer;
    std::ostringstream os;
    printer.printRowCount(os, 2);
    EXPECT_EQ(os.str(), "(2 rows)\n");
    
    std::ostringstream os2;
    printer.printRowCount(os2, 1);
    EXPECT_EQ(os2.str(), "(1 row)\n");
}

} // namespace hamdb::shell
