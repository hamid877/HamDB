#include "storage/disk_manager.hpp"
#include "storage/database_metadata.hpp"
#include "common/constants.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <random>

namespace hamdb
{

    // ── helpers (file-local) ─────────────────────────────────────────────────

    namespace
    {
        /// Fill @p buf with 16 cryptographically-random bytes using the OS
        /// random_device.  Falls back to a deterministic seed on failure, which
        /// is acceptable for UUID uniqueness but not for security.
        void generateUuid(std::array<std::uint8_t, 16>& buf)
        {
            std::random_device rd;
            std::mt19937_64 gen(rd());
            std::uniform_int_distribution<std::uint64_t> dist;

            std::uint64_t hi = dist(gen);
            std::uint64_t lo = dist(gen);
            std::memcpy(buf.data(), &hi, 8);
            std::memcpy(buf.data() + 8, &lo, 8);

            // Set UUID version 4 bits (RFC 4122)
            buf[6] = static_cast<std::uint8_t>((buf[6] & 0x0Fu) | 0x40u);
            // Set variant bits
            buf[8] = static_cast<std::uint8_t>((buf[8] & 0x3Fu) | 0x80u);
        }

        /// Return current Unix time in seconds.
        std::uint64_t unixTimestampNow()
        {
            using namespace std::chrono;
            return static_cast<std::uint64_t>(
                duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
        }
    } // anonymous namespace

    // ── Construction / Destruction ───────────────────────────────────────────

    DiskManager::DiskManager(const std::filesystem::path& path)
        : path_(path), stream_(), page_count_(0), is_open_(false)
    {
    }

    DiskManager::~DiskManager()
    {
        if (is_open_)
        {
            static_cast<void>(closeDatabase()); // best-effort; errors unrecoverable in dtor
        }
    }

    // ── Lifecycle ────────────────────────────────────────────────────────────

    Status DiskManager::createDatabase()
    {
        // Refuse to overwrite an existing file
        if (std::filesystem::exists(path_))
        {
            return Status::AlreadyExists;
        }

        // Build a zeroed 4096-byte page buffer
        std::array<std::byte, kPageSize> page_buf{};

        // Populate DatabaseMetadata
        DatabaseMetadata meta;

        // magic: copy exactly 8 chars from kDbMagic (no NUL)
        std::memcpy(meta.magic.data(), kDbMagic.data(), meta.magic.size());
        meta.version       = kFormatVersion;
        meta.reserved0     = 0;
        meta.page_size     = static_cast<std::uint32_t>(kPageSize);
        meta.page_count    = 1;
        meta.free_page_ptr = kInvalidPageId;
        meta.reserved1     = 0;
        generateUuid(meta.uuid);
        meta.created_at = unixTimestampNow();
        meta.reserved2  = 0;

        // Serialise into the first 64 bytes of the page buffer
        std::array<std::uint8_t, DatabaseMetadata::kSize> meta_buf{};
        meta.serialize(meta_buf);
        std::memcpy(page_buf.data(), meta_buf.data(), DatabaseMetadata::kSize);

        // Write the single page to disk
        std::ofstream out(path_, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
        {
            return Status::IoError;
        }

        out.write(reinterpret_cast<const char*>(page_buf.data()),
                  static_cast<std::streamsize>(kPageSize));

        if (!out.good())
        {
            return Status::IoError;
        }

        out.flush();
        out.close();

        return Status::Ok;
    }

    Status DiskManager::openDatabase()
    {
        if (!std::filesystem::exists(path_))
        {
            return Status::NotFound;
        }

        // Verify the file is large enough to hold at least one page
        std::error_code ec;
        const auto file_size = std::filesystem::file_size(path_, ec);
        if (ec || file_size < kPageSize)
        {
            return Status::Corruption;
        }

        // Open in binary read/write mode
        stream_.open(path_, std::ios::binary | std::ios::in | std::ios::out);
        if (!stream_.is_open())
        {
            return Status::IoError;
        }

        // Read the first DatabaseMetadata::kSize bytes of page 0
        std::array<std::uint8_t, DatabaseMetadata::kSize> meta_buf{};
        stream_.seekg(0);
        stream_.read(reinterpret_cast<char*>(meta_buf.data()),
                     static_cast<std::streamsize>(DatabaseMetadata::kSize));

        if (!stream_.good())
        {
            stream_.close();
            return Status::IoError;
        }

        // Deserialise and validate
        DatabaseMetadata meta;
        meta.deserialize(meta_buf);

        if (!meta.hasMagic())
        {
            stream_.close();
            return Status::Corruption;
        }

        if (!meta.hasValidVersion())
        {
            stream_.close();
            return Status::Corruption;
        }

        page_count_ = static_cast<std::size_t>(meta.page_count);
        is_open_    = true;

        return Status::Ok;
    }

    Status DiskManager::closeDatabase()
    {
        if (!is_open_)
        {
            return Status::Ok; // already closed — no-op
        }

        if (stream_.is_open())
        {
            stream_.flush();
            if (!stream_.good())
            {
                stream_.close();
                is_open_    = false;
                page_count_ = 0;
                return Status::IoError;
            }
            stream_.close();
        }

        is_open_    = false;
        page_count_ = 0;

        return Status::Ok;
    }

    // ── Core I/O ─────────────────────────────────────────────────────────────

    Status DiskManager::readPage([[maybe_unused]] PageId page_id,
                                 [[maybe_unused]] Page& page)
    {
        // TODO (Milestone 2): implement with stream_.seekg / stream_.read
        return Status::NotSupported;
    }

    Status DiskManager::writePage([[maybe_unused]] PageId page_id,
                                  [[maybe_unused]] const Page& page)
    {
        // TODO (Milestone 2): implement with stream_.seekp / stream_.write
        return Status::NotSupported;
    }

    Status DiskManager::allocatePage([[maybe_unused]] PageId& new_page_id)
    {
        // TODO (Milestone 2): extend file by kPageSize bytes
        return Status::NotSupported;
    }

    // ── Metadata ─────────────────────────────────────────────────────────────

    std::size_t DiskManager::pageCount() const
    {
        return page_count_;
    }

    const std::filesystem::path& DiskManager::filePath() const
    {
        return path_;
    }

    bool DiskManager::isOpen() const
    {
        return is_open_;
    }

    Status DiskManager::sync()
    {
        if (!is_open_ || !stream_.is_open())
        {
            return Status::Ok;
        }
        stream_.flush();
        return stream_.good() ? Status::Ok : Status::IoError;
    }

} // namespace hamdb
