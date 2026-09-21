#include "storage/disk_manager.hpp"
#include <stdexcept>

namespace hamdb
{

    DiskManager::DiskManager([[maybe_unused]] const std::filesystem::path& path)
        : path_(path), fd_(-1), page_count_(0)
    {
        // TODO (Milestone 2): open/create the database file with POSIX open().
    }

    DiskManager::~DiskManager() = default;
    {
        // TODO (Milestone 2): close fd_ if it is open.
    }

    Status DiskManager::readPage([[maybe_unused]] PageId page_id, [[maybe_unused]] Page& page)
    {
        // TODO (Milestone 2): pread() kPageSize bytes at page_id * kPageSize.
        return Status::NotSupported;
    }

    Status DiskManager::writePage([[maybe_unused]] PageId page_id,
                                  [[maybe_unused]] const Page& page)
    {
        // TODO (Milestone 2): pwrite() kPageSize bytes at page_id * kPageSize.
        return Status::NotSupported;
    }

    Status DiskManager::allocatePage([[maybe_unused]] PageId& new_page_id)
    {
        // TODO (Milestone 2): ftruncate() to extend file, update page_count_.
        return Status::NotSupported;
    }

    std::size_t DiskManager::pageCount() const
    {
        return page_count_;
    }

    const std::filesystem::path& DiskManager::filePath() const
    {
        return path_;
    }

    Status DiskManager::sync()
    {
        // TODO (Milestone 2): fsync(fd_).
        return Status::NotSupported;
    }

} // namespace hamdb
