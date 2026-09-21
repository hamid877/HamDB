#include "database/database.hpp"

namespace hamdb
{

    Database::Database([[maybe_unused]] const std::filesystem::path& path,
                       [[maybe_unused]] const Config& config)
        : path_(path), config_(config)
    {
        // TODO (future milestone): open or create the database file,
        // initialise the DiskManager, BufferPool, and Catalog.
    }

    Database::~Database()
    {
        // TODO (future milestone): flush dirty pages, release locks, close files.
    }

} // namespace hamdb
