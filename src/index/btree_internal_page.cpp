#include "index/btree_internal_page.hpp"

namespace hamdb
{

    void BTreeInternalPage::init(PageId page_id, PageId parent_page_id) noexcept
    {
        header_.setPageType(PageType::BTreeInternal);
        header_.setPageId(page_id);
        header_.setParentPageId(parent_page_id);
        header_.setCurrentSize(0);
        header_.setMaxSize(static_cast<uint16_t>(kMaxEntries));
    }

    uint16_t BTreeInternalPage::size() const noexcept
    {
        return header_.currentSize();
    }

    uint16_t BTreeInternalPage::maxSize() const noexcept
    {
        return header_.maxSize();
    }

    bool BTreeInternalPage::isEmpty() const noexcept
    {
        return size() == 0;
    }

    bool BTreeInternalPage::isFull() const noexcept
    {
        return header_.isFull();
    }

    PageId BTreeInternalPage::pageId() const noexcept
    {
        return header_.pageId();
    }

    PageId BTreeInternalPage::parentPageId() const noexcept
    {
        return header_.parentPageId();
    }

    int64_t BTreeInternalPage::keyAt(uint16_t index) const noexcept
    {
        // index 0 has no valid key, but we allow accessing it
        return entries_[index].key;
    }

    PageId BTreeInternalPage::childAt(uint16_t index) const noexcept
    {
        return entries_[index].page_id;
    }

    PageId BTreeInternalPage::lookup(int64_t key) const noexcept
    {
        if (isEmpty())
        {
            return kInvalidPageId;
        }

        int32_t left  = 1;
        int32_t right = static_cast<int32_t>(size()) - 1;
        int32_t ans   = static_cast<int32_t>(size());

        while (left <= right)
        {
            int32_t mid = left + (right - left) / 2;
            if (entries_[mid].key > key)
            {
                ans   = mid;
                right = mid - 1;
            }
            else
            {
                left = mid + 1;
            }
        }

        return entries_[ans - 1].page_id;
    }

    void BTreeInternalPage::populateNewRoot(PageId left_child, int64_t key, PageId right_child) noexcept
    {
        entries_[0].page_id = left_child;
        entries_[1].key     = key;
        entries_[1].page_id = right_child;
        header_.setCurrentSize(2);
    }

    Status BTreeInternalPage::insert(int64_t key, PageId child_page_id) noexcept
    {
        if (isFull())
        {
            return Status::InvalidArg;
        }

        int32_t left  = 1;
        int32_t right = static_cast<int32_t>(size()) - 1;
        int32_t idx   = static_cast<int32_t>(size());

        while (left <= right)
        {
            int32_t mid = left + (right - left) / 2;
            if (entries_[mid].key == key)
            {
                return Status::AlreadyExists;
            }
            if (entries_[mid].key > key)
            {
                idx   = mid;
                right = mid - 1;
            }
            else
            {
                left = mid + 1;
            }
        }

        for (int32_t i = static_cast<int32_t>(size()); i > idx; --i)
        {
            entries_[i] = entries_[i - 1];
        }

        entries_[idx].key     = key;
        entries_[idx].page_id = child_page_id;
        header_.setCurrentSize(size() + 1);
        return Status::Ok;
    }

    Status BTreeInternalPage::remove(int64_t key) noexcept
    {
        if (isEmpty())
        {
            return Status::NotFound;
        }

        int32_t left       = 1;
        int32_t right      = static_cast<int32_t>(size()) - 1;
        int32_t target_idx = -1;

        while (left <= right)
        {
            int32_t mid = left + (right - left) / 2;
            if (entries_[mid].key == key)
            {
                target_idx = mid;
                break;
            }
            if (entries_[mid].key > key)
            {
                right = mid - 1;
            }
            else
            {
                left = mid + 1;
            }
        }

        if (target_idx == -1)
        {
            return Status::NotFound;
        }

        for (int32_t i = target_idx; i < static_cast<int32_t>(size()) - 1; ++i)
        {
            entries_[i] = entries_[i + 1];
        }

        header_.setCurrentSize(size() - 1);
        return Status::Ok;
    }

    Status BTreeInternalPage::serializeEntry(Serializer& ser, const InternalEntry& entry) noexcept
    {
        const auto raw_key = static_cast<uint64_t>(entry.key);
        if (ser.writeUInt32(static_cast<uint32_t>(raw_key & 0xFFFF'FFFFu)) != Status::Ok)
        {
            return Status::IoError;
        }
        if (ser.writeUInt32(static_cast<uint32_t>(raw_key >> 32u)) != Status::Ok)
        {
            return Status::IoError;
        }
        if (ser.writeUInt32(entry.page_id) != Status::Ok)
        {
            return Status::IoError;
        }
        return Status::Ok;
    }

    Status BTreeInternalPage::deserializeEntry(Deserializer& de, InternalEntry& entry) noexcept
    {
        uint32_t lo = 0;
        uint32_t hi = 0;
        uint32_t pid = 0;

        if (de.readUInt32(lo) != Status::Ok) { return Status::IoError; }
        if (de.readUInt32(hi) != Status::Ok) { return Status::IoError; }
        if (de.readUInt32(pid) != Status::Ok) { return Status::IoError; }

        const uint64_t raw_key = (static_cast<uint64_t>(hi) << 32u) | lo;
        entry.key = static_cast<int64_t>(raw_key);
        entry.page_id = pid;
        return Status::Ok;
    }

    Status BTreeInternalPage::serialize(std::span<std::byte> dest) const noexcept
    {
        if (dest.size() < kHeaderSize + size() * kEntrySize)
        {
            return Status::IoError;
        }

        if (auto s = header_.serialize(dest.first(kHeaderSize)); s != Status::Ok)
        {
            return s;
        }

        Serializer ser(dest.subspan(kHeaderSize));
        for (uint16_t i = 0; i < size(); ++i)
        {
            if (auto s = serializeEntry(ser, entries_[i]); s != Status::Ok)
            {
                return s;
            }
        }

        return Status::Ok;
    }

    Status BTreeInternalPage::deserialize(std::span<const std::byte> src) noexcept
    {
        if (src.size() < kHeaderSize)
        {
            return Status::IoError;
        }

        if (auto s = header_.deserialize(src.first(kHeaderSize)); s != Status::Ok)
        {
            return s;
        }

        if (src.size() < kHeaderSize + size() * kEntrySize)
        {
            return Status::IoError;
        }

        Deserializer de(src.subspan(kHeaderSize));
        for (uint16_t i = 0; i < size(); ++i)
        {
            if (auto s = deserializeEntry(de, entries_[i]); s != Status::Ok)
            {
                return s;
            }
        }

        return Status::Ok;
    }

} // namespace hamdb
