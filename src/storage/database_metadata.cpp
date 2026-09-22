#include "storage/database_metadata.hpp"
#include "common/constants.hpp"
#include <cstring>

namespace hamdb
{

    void DatabaseMetadata::serialize(std::array<std::uint8_t, kSize>& out) const noexcept
    {
        std::size_t offset = 0;

        // magic (8 bytes)
        std::memcpy(out.data() + offset, magic.data(), magic.size());
        offset += magic.size(); // 8

        // version (2 bytes, little-endian)
        std::memcpy(out.data() + offset, &version, sizeof(version));
        offset += sizeof(version); // 10

        // reserved0 (2 bytes)
        std::memcpy(out.data() + offset, &reserved0, sizeof(reserved0));
        offset += sizeof(reserved0); // 12

        // page_size (4 bytes)
        std::memcpy(out.data() + offset, &page_size, sizeof(page_size));
        offset += sizeof(page_size); // 16

        // page_count (8 bytes)
        std::memcpy(out.data() + offset, &page_count, sizeof(page_count));
        offset += sizeof(page_count); // 24

        // free_page_ptr (4 bytes)
        std::memcpy(out.data() + offset, &free_page_ptr, sizeof(free_page_ptr));
        offset += sizeof(free_page_ptr); // 28

        // reserved1 (4 bytes)
        std::memcpy(out.data() + offset, &reserved1, sizeof(reserved1));
        offset += sizeof(reserved1); // 32

        // uuid (16 bytes)
        std::memcpy(out.data() + offset, uuid.data(), uuid.size());
        offset += uuid.size(); // 48

        // created_at (8 bytes)
        std::memcpy(out.data() + offset, &created_at, sizeof(created_at));
        offset += sizeof(created_at); // 56

        // reserved2 (8 bytes)
        std::memcpy(out.data() + offset, &reserved2, sizeof(reserved2));
        // offset = 64
    }

    void DatabaseMetadata::deserialize(const std::array<std::uint8_t, kSize>& in) noexcept
    {
        std::size_t offset = 0;

        std::memcpy(magic.data(), in.data() + offset, magic.size());
        offset += magic.size();

        std::memcpy(&version, in.data() + offset, sizeof(version));
        offset += sizeof(version);

        std::memcpy(&reserved0, in.data() + offset, sizeof(reserved0));
        offset += sizeof(reserved0);

        std::memcpy(&page_size, in.data() + offset, sizeof(page_size));
        offset += sizeof(page_size);

        std::memcpy(&page_count, in.data() + offset, sizeof(page_count));
        offset += sizeof(page_count);

        std::memcpy(&free_page_ptr, in.data() + offset, sizeof(free_page_ptr));
        offset += sizeof(free_page_ptr);

        std::memcpy(&reserved1, in.data() + offset, sizeof(reserved1));
        offset += sizeof(reserved1);

        std::memcpy(uuid.data(), in.data() + offset, uuid.size());
        offset += uuid.size();

        std::memcpy(&created_at, in.data() + offset, sizeof(created_at));
        offset += sizeof(created_at);

        std::memcpy(&reserved2, in.data() + offset, sizeof(reserved2));
    }

    bool DatabaseMetadata::hasMagic() const noexcept
    {
        // Compare 8 bytes of magic without assuming NUL termination
        return std::string_view(magic.data(), magic.size()) == kMagicValue;
    }

    bool DatabaseMetadata::hasValidVersion() const noexcept
    {
        return version == kFormatVersion;
    }

} // namespace hamdb
