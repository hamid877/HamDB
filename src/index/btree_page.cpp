/// @file btree_page.cpp
/// @brief Implementation of BTreePage — the shared B+ Tree node header.

#include "index/btree_page.hpp"

#include <array>
#include <cstdint>

namespace hamdb
{

    // ── Construction ──────────────────────────────────────────────────────────

    BTreePage::BTreePage(PageType type, PageId page_id, PageId parent_page_id,
                         uint16_t max_size) noexcept
        : page_type_(type), current_size_(0), max_size_(max_size), parent_page_id_(parent_page_id),
          page_id_(page_id)
    {
    }

    // ── Getters ───────────────────────────────────────────────────────────────

    PageType BTreePage::pageType() const noexcept
    {
        return page_type_;
    }

    uint16_t BTreePage::currentSize() const noexcept
    {
        return current_size_;
    }

    uint16_t BTreePage::maxSize() const noexcept
    {
        return max_size_;
    }

    uint16_t BTreePage::minSize() const noexcept
    {
        if (page_type_ == PageType::BTreeLeaf)
        {
            return max_size_ / 2;
        }
        return (max_size_ + 1) / 2;
    }

    PageId BTreePage::parentPageId() const noexcept
    {
        return parent_page_id_;
    }

    PageId BTreePage::pageId() const noexcept
    {
        return page_id_;
    }

    bool BTreePage::isFull() const noexcept
    {
        return current_size_ >= max_size_;
    }

    bool BTreePage::isRoot() const noexcept
    {
        return parent_page_id_ == kInvalidPageId;
    }

    // ── Setters ───────────────────────────────────────────────────────────────

    void BTreePage::setPageType(PageType type) noexcept
    {
        page_type_ = type;
    }

    void BTreePage::setCurrentSize(uint16_t size) noexcept
    {
        current_size_ = size;
    }

    void BTreePage::setMaxSize(uint16_t size) noexcept
    {
        max_size_ = size;
    }

    void BTreePage::setParentPageId(PageId id) noexcept
    {
        parent_page_id_ = id;
    }

    void BTreePage::setPageId(PageId id) noexcept
    {
        page_id_ = id;
    }

    // ── Serialisation ─────────────────────────────────────────────────────────

    Status BTreePage::serialize(std::span<std::byte> dest) const noexcept
    {
        if (dest.size() < kHeaderSize)
        {
            return Status::IoError;
        }

        Serializer ser(dest.subspan(0, kHeaderSize));

        // Byte 0: page_type (uint8)
        if (ser.writeUInt8(static_cast<uint8_t>(page_type_)) != Status::Ok)
        {
            return Status::IoError;
        }

        // Bytes 1-2: current_size (uint16, LE)
        if (ser.writeUInt16(current_size_) != Status::Ok)
        {
            return Status::IoError;
        }

        // Bytes 3-4: max_size (uint16, LE)
        if (ser.writeUInt16(max_size_) != Status::Ok)
        {
            return Status::IoError;
        }

        // Bytes 5-8: parent_page_id (uint32, LE)
        if (ser.writeUInt32(parent_page_id_) != Status::Ok)
        {
            return Status::IoError;
        }

        // Bytes 9-12: page_id (uint32, LE)
        if (ser.writeUInt32(page_id_) != Status::Ok)
        {
            return Status::IoError;
        }

        // Bytes 13-15: reserved — zero-fill (3 bytes)
        constexpr std::array<uint8_t, 3> kReserved = {0, 0, 0};
        const auto reserved_bytes = std::as_bytes(std::span<const uint8_t, 3>{kReserved});
        if (ser.writeBytes(reserved_bytes) != Status::Ok)
        {
            return Status::IoError;
        }

        return Status::Ok;
    }

    Status BTreePage::deserialize(std::span<const std::byte> src) noexcept
    {
        if (src.size() < kHeaderSize)
        {
            return Status::IoError;
        }

        Deserializer de(src.subspan(0, kHeaderSize));

        uint8_t raw_type = 0;
        uint16_t cur_size = 0;
        uint16_t max_sz = 0;
        uint32_t parent_id = 0;
        uint32_t pid = 0;

        if (de.readUInt8(raw_type) != Status::Ok)
        {
            return Status::IoError;
        }
        if (de.readUInt16(cur_size) != Status::Ok)
        {
            return Status::IoError;
        }
        if (de.readUInt16(max_sz) != Status::Ok)
        {
            return Status::IoError;
        }
        if (de.readUInt32(parent_id) != Status::Ok)
        {
            return Status::IoError;
        }
        if (de.readUInt32(pid) != Status::Ok)
        {
            return Status::IoError;
        }
        // Consume reserved bytes (3 bytes) — value is discarded.
        std::span<const std::byte> reserved_view;
        if (de.readBytes(3, reserved_view) != Status::Ok)
        {
            return Status::IoError;
        }

        // Commit only after all reads succeed.
        page_type_ = static_cast<PageType>(raw_type);
        current_size_ = cur_size;
        max_size_ = max_sz;
        parent_page_id_ = static_cast<PageId>(parent_id);
        page_id_ = static_cast<PageId>(pid);

        return Status::Ok;
    }

    // ── Comparison ────────────────────────────────────────────────────────────

    bool BTreePage::operator==(const BTreePage& other) const noexcept
    {
        return page_type_ == other.page_type_ && current_size_ == other.current_size_ &&
               max_size_ == other.max_size_ && parent_page_id_ == other.parent_page_id_ &&
               page_id_ == other.page_id_;
    }

    bool BTreePage::operator!=(const BTreePage& other) const noexcept
    {
        return !(*this == other);
    }

} // namespace hamdb
