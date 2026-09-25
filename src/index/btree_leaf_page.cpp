/// @file btree_leaf_page.cpp
/// @brief Implementation of BTreeLeafPage — the B+ Tree leaf node.

#include "index/btree_leaf_page.hpp"

#include <algorithm>
#include <cstdint>

namespace hamdb
{

    // ── Private static helpers ────────────────────────────────────────────────────

    Status BTreeLeafPage::serializeEntry(Serializer& ser, const LeafEntry& entry) noexcept
    {
        // Key: int64, stored as two uint32 halves to avoid signed-cast pitfalls.
        const auto raw_key = static_cast<uint64_t>(entry.key);
        if (ser.writeUInt32(static_cast<uint32_t>(raw_key & 0xFFFF'FFFFu)) != Status::Ok)
        {
            return Status::IoError;
        }
        if (ser.writeUInt32(static_cast<uint32_t>(raw_key >> 32u)) != Status::Ok)
        {
            return Status::IoError;
        }
        // RID: page_id (uint32) + slot_id (uint16)
        if (ser.writeUInt32(entry.rid.getPageId()) != Status::Ok)
        {
            return Status::IoError;
        }
        if (ser.writeUInt16(entry.rid.getSlotId()) != Status::Ok)
        {
            return Status::IoError;
        }
        return Status::Ok;
    }

    Status BTreeLeafPage::deserializeEntry(Deserializer& de, LeafEntry& entry) noexcept
    {
        uint32_t lo = 0;
        uint32_t hi = 0;
        uint32_t pid = 0;
        uint16_t slot = 0;

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
        if (de.readUInt16(slot) != Status::Ok)
        {
            return Status::IoError;
        }

        const uint64_t raw_key = (static_cast<uint64_t>(hi) << 32u) | static_cast<uint64_t>(lo);
        entry.key = static_cast<int64_t>(raw_key);
        entry.rid = RID{pid, slot};
        return Status::Ok;
    }

    // ── Init ──────────────────────────────────────────────────────────────────────

    void BTreeLeafPage::init(PageId page_id, PageId parent_page_id) noexcept
    {
        const auto max_sz = static_cast<uint16_t>(kMaxEntries);
        header_ = BTreePage(PageType::BTreeLeaf, page_id, parent_page_id, max_sz);
        prev_page_id_ = kInvalidPageId;
        next_page_id_ = kInvalidPageId;
    }

    // ── Header accessors ──────────────────────────────────────────────────────────

    PageId BTreeLeafPage::prevPageId() const noexcept
    {
        return prev_page_id_;
    }

    PageId BTreeLeafPage::nextPageId() const noexcept
    {
        return next_page_id_;
    }

    void BTreeLeafPage::setPrevPageId(PageId id) noexcept
    {
        prev_page_id_ = id;
    }

    void BTreeLeafPage::setNextPageId(PageId id) noexcept
    {
        next_page_id_ = id;
    }

    // ── Delegated BTreePage accessors ─────────────────────────────────────────────

    uint16_t BTreeLeafPage::size() const noexcept
    {
        return header_.currentSize();
    }

    uint16_t BTreeLeafPage::maxSize() const noexcept
    {
        return header_.maxSize();
    }

    uint16_t BTreeLeafPage::minSize() const noexcept
    {
        return header_.minSize();
    }

    bool BTreeLeafPage::isEmpty() const noexcept
    {
        return header_.currentSize() == 0;
    }

    bool BTreeLeafPage::isFull() const noexcept
    {
        return header_.isFull();
    }

    PageId BTreeLeafPage::pageId() const noexcept
    {
        return header_.pageId();
    }

    PageId BTreeLeafPage::parentPageId() const noexcept
    {
        return header_.parentPageId();
    }

    void BTreeLeafPage::setParentPageId(PageId id) noexcept
    {
        header_.setParentPageId(id);
    }

    // ── Slot accessors ────────────────────────────────────────────────────────────

    int64_t BTreeLeafPage::keyAt(uint16_t index) const noexcept
    {
        return entries_[index].key;
    }

    RID BTreeLeafPage::valueAt(uint16_t index) const noexcept
    {
        return entries_[index].rid;
    }

    // ── Private helpers ───────────────────────────────────────────────────────────

    int32_t BTreeLeafPage::findIndex(int64_t key) const noexcept
    {
        const uint16_t count = header_.currentSize();
        if (count == 0)
        {
            return -1;
        }

        // Standard binary search over [0, count).
        int32_t lo = 0;
        int32_t hi = static_cast<int32_t>(count) - 1;

        while (lo <= hi)
        {
            const int32_t mid = lo + (hi - lo) / 2;
            const int64_t k = entries_[static_cast<std::size_t>(mid)].key;

            if (k == key)
            {
                return mid;
            }
            if (k < key)
            {
                lo = mid + 1;
            }
            else
            {
                hi = mid - 1;
            }
        }
        return -1; // not found
    }

    // ── Lookup ────────────────────────────────────────────────────────────────────

    std::optional<RID> BTreeLeafPage::lookup(int64_t key) const noexcept
    {
        const int32_t idx = findIndex(key);
        if (idx < 0)
        {
            return std::nullopt;
        }
        return entries_[static_cast<std::size_t>(idx)].rid;
    }

    // ── Mutation ──────────────────────────────────────────────────────────────────

    Status BTreeLeafPage::insert(int64_t key, RID rid) noexcept
    {
        if (header_.isFull())
        {
            return Status::InvalidArg;
        }

        const uint16_t count = header_.currentSize();

        // Find insertion position (lower_bound over entries_[0..count)).
        uint16_t pos = 0;
        {
            int32_t lo = 0;
            int32_t hi = static_cast<int32_t>(count);
            while (lo < hi)
            {
                const int32_t mid = lo + (hi - lo) / 2;
                if (entries_[static_cast<std::size_t>(mid)].key < key)
                {
                    lo = mid + 1;
                }
                else
                {
                    hi = mid;
                }
            }
            pos = static_cast<uint16_t>(lo);
        }

        // Reject duplicate keys.
        if (pos < count && entries_[pos].key == key)
        {
            return Status::AlreadyExists;
        }

        // Shift entries right-to-left to make room.
        std::move_backward(entries_.begin() + pos, entries_.begin() + count,
                           entries_.begin() + count + 1);

        entries_[pos] = LeafEntry{key, rid};
        header_.setCurrentSize(static_cast<uint16_t>(count + 1u));
        return Status::Ok;
    }

    Status BTreeLeafPage::remove(int64_t key) noexcept
    {
        const int32_t idx = findIndex(key);
        if (idx < 0)
        {
            return Status::NotFound;
        }

        const uint16_t count = header_.currentSize();
        const auto pos = static_cast<uint16_t>(idx);

        // Shift entries left to close the gap.
        for (uint16_t i = pos; i < static_cast<uint16_t>(count - 1u); ++i)
        {
            entries_[i] = entries_[i + 1u];
        }

        header_.setCurrentSize(static_cast<uint16_t>(count - 1u));
        return Status::Ok;
    }

    void BTreeLeafPage::moveHalfTo(BTreeLeafPage& recipient) noexcept
    {
        const uint16_t total = header_.currentSize();
        const uint16_t half = total / 2;
        const uint16_t move_count = total - half;

        for (uint16_t i = 0; i < move_count; ++i)
        {
            recipient.entries_[i] = entries_[half + i];
        }

        recipient.header_.setCurrentSize(move_count);
        header_.setCurrentSize(half);
    }

    void BTreeLeafPage::moveFirstToEndOf(BTreeLeafPage& recipient) noexcept
    {
        recipient.entries_[recipient.size()] = entries_[0];
        recipient.header_.setCurrentSize(recipient.size() + 1);

        for (uint16_t i = 0; i < size() - 1; ++i)
        {
            entries_[i] = entries_[i + 1];
        }
        header_.setCurrentSize(size() - 1);
    }

    void BTreeLeafPage::moveLastToFrontOf(BTreeLeafPage& recipient) noexcept
    {
        for (int i = recipient.size(); i > 0; --i)
        {
            recipient.entries_[i] = recipient.entries_[i - 1];
        }
        recipient.entries_[0] = entries_[size() - 1];
        recipient.header_.setCurrentSize(recipient.size() + 1);
        header_.setCurrentSize(size() - 1);
    }

    void BTreeLeafPage::moveAllTo(BTreeLeafPage& recipient) noexcept
    {
        uint16_t rec_size = recipient.size();
        for (uint16_t i = 0; i < size(); ++i)
        {
            recipient.entries_[rec_size + i] = entries_[i];
        }
        recipient.header_.setCurrentSize(rec_size + size());
        header_.setCurrentSize(0);
    }

    // ── Serialisation ─────────────────────────────────────────────────────────────

    Status BTreeLeafPage::serialize(std::span<std::byte> dest) const noexcept
    {
        const uint16_t count = header_.currentSize();
        const std::size_t needed = kLeafHeaderSize + static_cast<std::size_t>(count) * kEntrySize;

        if (dest.size() < needed)
        {
            return Status::IoError;
        }

        // 1. Serialise the BTreePage sub-header (16 bytes).
        if (header_.serialize(dest.subspan(0, BTreePage::kHeaderSize)) != Status::Ok)
        {
            return Status::IoError;
        }

        // 2. Serialise sibling pointers (8 bytes) after the BTreePage header.
        Serializer ser(dest.subspan(BTreePage::kHeaderSize, kSiblingPtrSize));
        if (ser.writeUInt32(prev_page_id_) != Status::Ok)
        {
            return Status::IoError;
        }
        if (ser.writeUInt32(next_page_id_) != Status::Ok)
        {
            return Status::IoError;
        }

        // 3. Serialise entries.
        Serializer eser(
            dest.subspan(kLeafHeaderSize, static_cast<std::size_t>(count) * kEntrySize));
        for (uint16_t i = 0; i < count; ++i)
        {
            if (serializeEntry(eser, entries_[i]) != Status::Ok)
            {
                return Status::IoError;
            }
        }

        return Status::Ok;
    }

    Status BTreeLeafPage::deserialize(std::span<const std::byte> src) noexcept
    {
        if (src.size() < kLeafHeaderSize)
        {
            return Status::IoError;
        }

        // 1. Read the BTreePage sub-header.
        BTreePage tmp_header;
        if (tmp_header.deserialize(src.subspan(0, BTreePage::kHeaderSize)) != Status::Ok)
        {
            return Status::IoError;
        }

        const uint16_t count = tmp_header.currentSize();
        const std::size_t needed = kLeafHeaderSize + static_cast<std::size_t>(count) * kEntrySize;

        if (src.size() < needed)
        {
            return Status::IoError;
        }

        // 2. Read sibling pointers.
        Deserializer de(src.subspan(BTreePage::kHeaderSize, kSiblingPtrSize));
        uint32_t prev = 0;
        uint32_t next = 0;
        if (de.readUInt32(prev) != Status::Ok)
        {
            return Status::IoError;
        }
        if (de.readUInt32(next) != Status::Ok)
        {
            return Status::IoError;
        }

        // 3. Read entries.
        std::array<LeafEntry, kMaxEntries> tmp_entries{};
        if (count > 0)
        {
            Deserializer ede(
                src.subspan(kLeafHeaderSize, static_cast<std::size_t>(count) * kEntrySize));
            for (uint16_t i = 0; i < count; ++i)
            {
                if (deserializeEntry(ede, tmp_entries[i]) != Status::Ok)
                {
                    return Status::IoError;
                }
            }
        }

        // Commit — all reads succeeded.
        header_ = tmp_header;
        prev_page_id_ = prev;
        next_page_id_ = next;
        entries_ = tmp_entries;
        return Status::Ok;
    }

} // namespace hamdb
