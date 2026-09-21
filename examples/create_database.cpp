/**
 * @file create_database.cpp
 * @brief Minimal example demonstrating HamDB scaffolding.
 *
 * This example shows how the public API surface will look once Milestone 2
 * (storage logic) is implemented.  For now it only exercises the type system
 * and prints the page constants to stdout.
 *
 * Compile (from the build directory):
 * @code
 *   cmake --build . --target hamdb_example_create_database
 *   ./examples/hamdb_example_create_database
 * @endcode
 */

#include "common/types.h"
#include "storage/page.h"
#include "storage/page_header.h"
#include "storage/disk_manager.h"

#include <filesystem>
#include <iostream>

int main() {
    std::cout << "=== HamDB — Milestone 1 scaffolding demo ===\n\n";

    // ── 1. Print page constants ────────────────────────────────────────────────
    std::cout << "Page size    : " << hamdb::kPageSize          << " bytes\n";
    std::cout << "Header size  : " << hamdb::PageHeader::kSize   << " bytes\n";
    std::cout << "Body size    : " << hamdb::Page::kBodySize     << " bytes\n";
    std::cout << "Invalid page : " << hamdb::kInvalidPageId      << "\n\n";

    // ── 2. Construct a page header ─────────────────────────────────────────────
    hamdb::PageHeader hdr(0, hamdb::PageHeader::PageType::Data);
    std::cout << "PageHeader constructed: page_id=" << hdr.page_id << "\n";

    // ── 3. Construct a page ───────────────────────────────────────────────────
    hamdb::Page page(hdr);
    std::cout << "Page constructed      : id="   << page.id()
              << ", data span size="             << page.data().size() << "\n";

    // ── 4. Construct a disk manager (stub) ────────────────────────────────────
    auto db_path = std::filesystem::temp_directory_path() / "example.hamdb";
    hamdb::DiskManager dm(db_path);
    std::cout << "DiskManager           : path=" << dm.filePath()
              << ", pages="                      << dm.pageCount() << "\n";

    // ── 5. Show status helper ─────────────────────────────────────────────────
    std::cout << "\nStatus::Ok          -> " << hamdb::statusToString(hamdb::Status::Ok)    << "\n";
    std::cout << "Status::NotSupported -> " << hamdb::statusToString(hamdb::Status::NotSupported) << "\n";

    std::cout << "\nMilestone 1 scaffolding OK.\n";
    return 0;
}
