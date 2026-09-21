/**
 * @file create_database.cpp
 * @brief Minimal example demonstrating the HamDB Milestone 1.1 structure.
 *
 * Exercises the public API surface after the refactor:
 *  - constants.hpp (page geometry, magic)
 *  - enums.hpp (Status, PageType)
 *  - storage/page_header.hpp
 *  - storage/page.hpp
 *  - storage/disk_manager.hpp
 *  - database/database.hpp
 *
 * Compile (from the build directory):
 * @code
 *   cmake --build . --target hamdb_example_create_database
 *   ./examples/hamdb_example_create_database
 * @endcode
 */

#include "common/constants.hpp"
#include "common/enums.hpp"
#include "database/database.hpp"
#include "storage/disk_manager.hpp"
#include "storage/page.hpp"
#include "storage/page_header.hpp"

#include <filesystem>
#include <iostream>

int main()
{
    std::cout << "=== HamDB — Milestone 1.1 refactor demo ===\n\n";

    // ── 1. Constants ───────────────────────────────────────────────────────────
    std::cout << "Magic        : " << hamdb::kMagic << "\n";
    std::cout << "Version      : " << hamdb::kFormatVersion << "\n";
    std::cout << "Page size    : " << hamdb::kPageSize << " bytes\n";
    std::cout << "Header size  : " << hamdb::kPageHeaderSize << " bytes\n";
    std::cout << "Body size    : " << hamdb::kPageBodySize << " bytes\n";
    std::cout << "Invalid page : " << hamdb::kInvalidPageId << "\n\n";

    // ── 2. PageType enum ───────────────────────────────────────────────────────
    std::cout << "PageType::Table    = " << static_cast<int>(hamdb::PageType::Table) << "\n";
    std::cout << "PageType::Metadata = " << static_cast<int>(hamdb::PageType::Metadata) << "\n\n";

    // ── 3. PageHeader ──────────────────────────────────────────────────────────
    hamdb::PageHeader hdr(0, hamdb::PageType::Table);
    std::cout << "PageHeader constructed: page_id=" << hdr.page_id << "\n";

    // ── 4. Page ────────────────────────────────────────────────────────────────
    hamdb::Page page(hdr);
    std::cout << "Page constructed      : id=" << page.id()
              << ", data span size=" << page.data().size() << "\n";

    // ── 5. DiskManager (stub) ─────────────────────────────────────────────────
    auto db_path = std::filesystem::temp_directory_path() / "example.hamdb";
    hamdb::DiskManager dm(db_path);
    std::cout << "DiskManager           : path=" << dm.filePath() << ", pages=" << dm.pageCount()
              << "\n";

    // ── 6. Database (stub) ────────────────────────────────────────────────────
    hamdb::Database db(db_path);
    std::cout << "Database constructed  : OK\n\n";

    // ── 7. Status helper ───────────────────────────────────────────────────────
    std::cout << "Status::Ok           -> " << hamdb::statusToString(hamdb::Status::Ok) << "\n";
    std::cout << "Status::NotSupported -> " << hamdb::statusToString(hamdb::Status::NotSupported)
              << "\n";

    std::cout << "\nMilestone 1.1 refactor OK.\n";
    return 0;
}
