#include "index/btree_internal_page.hpp"
#include "buffer/buffer_pool_manager.hpp"

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

    uint16_t BTreeInternalPage::minSize() const noexcept
    {
        return header_.minSize();
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

    void BTreeInternalPage::setKeyAt(uint16_t index, int64_t key) noexcept
    {
        entries_[index].key = key;
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

        int32_t left = 1;
        int32_t right = static_cast<int32_t>(size()) - 1;
        int32_t ans = static_cast<int32_t>(size());

        while (left <= right)
        {
            int32_t mid = left + (right - left) / 2;
            if (entries_[mid].key > key)
            {
                ans = mid;
                right = mid - 1;
            }
            else
            {
                left = mid + 1;
            }
        }

        return entries_[ans - 1].page_id;
    }

    void BTreeInternalPage::populateNewRoot(PageId left_child, int64_t key,
                                            PageId right_child) noexcept
    {
        entries_[0].page_id = left_child;
        entries_[1].key = key;
        entries_[1].page_id = right_child;
        header_.setCurrentSize(2);
    }

    Status BTreeInternalPage::insert(int64_t key, PageId child_page_id) noexcept
    {
        if (isFull())
        {
            return Status::InvalidArg;
        }

        int32_t left = 1;
        int32_t right = static_cast<int32_t>(size()) - 1;
        int32_t idx = static_cast<int32_t>(size());

        while (left <= right)
        {
            int32_t mid = left + (right - left) / 2;
            if (entries_[mid].key == key)
            {
                return Status::AlreadyExists;
            }
            if (entries_[mid].key > key)
            {
                idx = mid;
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

        entries_[idx].key = key;
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

        int32_t left = 1;
        int32_t right = static_cast<int32_t>(size()) - 1;
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

        if (de.readUInt32(lo) != Status::Ok)
        {
            return Status::IoError;
        }
        if (de.readUInt32(hi) != Status::Ok)
        {
            return Status::IoError;
        }
        if (de.readUInt32(pid) != Status::Ok)
        {
            return Status::IoError;
        }

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

    int64_t BTreeInternalPage::moveHalfTo(BTreeInternalPage& recipient) noexcept
    {
        int32_t start_idx = size() / 2;
        int64_t median_key = entries_[start_idx].key;

        int32_t j = 0;
        for (int32_t i = start_idx; i < size(); ++i)
        {
            recipient.entries_[j] = entries_[i];
            j++;
        }

        recipient.header_.setCurrentSize(j);
        header_.setCurrentSize(start_idx);

        return median_key;
    }

    int64_t BTreeInternalPage::moveFirstToEndOf(BTreeInternalPage& recipient,
                                                int64_t middle_key) noexcept
    {
        PageId child_id = entries_[0].page_id;
        recipient.entries_[recipient.size()].key = middle_key;
        recipient.entries_[recipient.size()].page_id = child_id;
        recipient.header_.setCurrentSize(recipient.size() + 1);

        int64_t new_middle_key = entries_[1].key;

        for (uint16_t i = 0; i < size() - 1; ++i)
        {
            entries_[i] = entries_[i + 1];
        }
        header_.setCurrentSize(size() - 1);

        return new_middle_key;
    }

    int64_t BTreeInternalPage::moveLastToFrontOf(BTreeInternalPage& recipient,
                                                 int64_t middle_key) noexcept
    {
        int64_t new_middle_key = entries_[size() - 1].key;
        PageId child_id = entries_[size() - 1].page_id;

        for (int i = recipient.size(); i > 0; --i)
        {
            recipient.entries_[i] = recipient.entries_[i - 1];
        }
        recipient.entries_[1].key = middle_key;
        recipient.entries_[0].page_id = child_id;
        recipient.header_.setCurrentSize(recipient.size() + 1);

        header_.setCurrentSize(size() - 1);

        return new_middle_key;
    }

    void BTreeInternalPage::moveAllTo(BTreeInternalPage& recipient, int64_t middle_key) noexcept
    {
        recipient.entries_[recipient.size()].key = middle_key;
        recipient.entries_[recipient.size()].page_id = entries_[0].page_id;

        uint16_t start_idx = recipient.size() + 1;
        for (uint16_t i = 1; i < size(); ++i)
        {
            recipient.entries_[start_idx + i - 1] = entries_[i];
        }

        recipient.header_.setCurrentSize(recipient.size() + size());
        header_.setCurrentSize(0);
    }

    int BTreeInternalPage::findChildIndex(PageId child_page_id) const noexcept
    {
        for (uint16_t i = 0; i < size(); ++i)
        {
            if (entries_[i].page_id == child_page_id)
            {
                return i;
            }
        }
        return -1;
    }

} // namespace hamdb
