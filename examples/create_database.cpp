/**
 * @file create_database.cpp
 * @brief Example: create a HamDB database file called `users.hamdb`.
 *
 * Demonstrates the Milestone 1.3 lifecycle API:
 *   1. Construct a DiskManager with the target path.
 *   2. Call createDatabase() to write the initial page.
 *   3. Call openDatabase() to verify the file and read its metadata.
 *   4. Print status to stdout and exit.
 *
 * Compile & run (from the build directory):
 * @code
 *   cmake --build . --target hamdb_example_create_database
 *   ./examples/hamdb_example_create_database
 * @endcode
 */

#include "common/constants.hpp"
#include "common/enums.hpp"
#include "storage/disk_manager.hpp"

#include <filesystem>
#include <iostream>

int main()
{
    std::cout << "=== HamDB — Milestone 1.3: create_database ===\n\n";

    // Target file in the current working directory
    const std::filesystem::path db_path = "users.hamdb";

    // ── 1. Create the database ────────────────────────────────────────────────
    {
        hamdb::DiskManager dm(db_path);
        const hamdb::Status status = dm.createDatabase();

        if (status == hamdb::Status::AlreadyExists)
        {
            std::cout << "[WARN] users.hamdb already exists — skipping creation.\n";
        }
        else if (status != hamdb::Status::Ok)
        {
            std::cerr << "[ERROR] createDatabase() failed: "
                      << hamdb::statusToString(status) << "\n";
            return 1;
        }
        else
        {
            std::cout << "[OK]   users.hamdb created successfully.\n";
        }
    }

    // ── 2. Open and inspect the database ─────────────────────────────────────
    {
        hamdb::DiskManager dm(db_path);
        const hamdb::Status status = dm.openDatabase();

        if (status != hamdb::Status::Ok)
        {
            std::cerr << "[ERROR] openDatabase() failed: "
                      << hamdb::statusToString(status) << "\n";
            return 1;
        }

        std::cout << "[OK]   users.hamdb opened successfully.\n";
        std::cout << "       Path       : " << dm.filePath().string() << "\n";
        std::cout << "       Page count : " << dm.pageCount() << "\n";
        std::cout << "       File size  : "
                  << std::filesystem::file_size(db_path) << " bytes\n";

        // closeDatabase() is called automatically by the destructor
    }

    std::cout << "\nDone.\n";
    return 0;
}
