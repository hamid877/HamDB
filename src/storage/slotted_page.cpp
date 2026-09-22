#include "storage/slotted_page.hpp"
#include "utils/deserializer.hpp"
#include "utils/serializer.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace hamdb
{

    // ── Construction ──────────────────────────────────────────────────────────

    SlottedPage::SlottedPage(Page& page) noexcept : page_(page) {}

    // ── Internal: SlottedPageHeader I/O ──────────────────────────────────────

    SlottedPageHeader SlottedPage::readSpHeader() const noexcept
    {
        // The SlottedPageHeader lives at bytes [0..7] of the body.
        auto body = page_.body(); // span<const byte> via const body()
        // Build a const view for Deserializer
        std::span<const std::byte> view(body.data(), SlottedPageHeader::kSize);
        Deserializer des(view);

        SlottedPageHeader h;
        std::uint16_t tmp = 0;

        static_cast<void>(des.readUInt16(tmp)); h.tuple_count      = tmp;
        static_cast<void>(des.readUInt16(tmp)); h.free_space_start = tmp;
        static_cast<void>(des.readUInt16(tmp)); h.free_space_end   = tmp;
        static_cast<void>(des.readUInt16(tmp)); h.reserved         = tmp;

        return h;
    }

    void SlottedPage::writeSpHeader(const SlottedPageHeader& h) noexcept
    {
        auto body = page_.body(); // span<byte>
        std::span<std::byte> view(body.data(), SlottedPageHeader::kSize);
        Serializer ser(view);

        static_cast<void>(ser.writeUInt16(h.tuple_count));
        static_cast<void>(ser.writeUInt16(h.free_space_start));
        static_cast<void>(ser.writeUInt16(h.free_space_end));
        static_cast<void>(ser.writeUInt16(h.reserved));
    }

    // ── Internal: TupleSlot I/O ───────────────────────────────────────────────

    TupleSlot SlottedPage::readSlot(std::uint16_t idx) const noexcept
    {
        auto body = page_.body();
        const std::size_t off = slotOffset(idx);
        std::span<const std::byte> view(body.data() + off, TupleSlot::kSize);
        Deserializer des(view);

        TupleSlot slot;
        std::uint16_t tmp = 0;
        static_cast<void>(des.readUInt16(tmp)); slot.offset   = tmp;
        static_cast<void>(des.readUInt16(tmp)); slot.length   = tmp;
        static_cast<void>(des.readUInt16(tmp)); slot.flags    = tmp;
        static_cast<void>(des.readUInt16(tmp)); slot.reserved = tmp;

        return slot;
    }

    void SlottedPage::writeSlot(std::uint16_t idx, const TupleSlot& slot) noexcept
    {
        auto body = page_.body();
        const std::size_t off = slotOffset(idx);
        std::span<std::byte> view(body.data() + off, TupleSlot::kSize);
        Serializer ser(view);

        static_cast<void>(ser.writeUInt16(slot.offset));
        static_cast<void>(ser.writeUInt16(slot.length));
        static_cast<void>(ser.writeUInt16(slot.flags));
        static_cast<void>(ser.writeUInt16(slot.reserved));
    }

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    Status SlottedPage::initialize() noexcept
    {
        // Zero the entire body
        auto body = page_.body();
        for (auto& b : body)
        {
            b = std::byte{0};
        }

        // The slot directory grows from the top; the initial freeSpaceStart is
        // right after the SlottedPageHeader.
        // Tuples grow from the bottom; freeSpaceEnd starts at body size.
        SlottedPageHeader h;
        h.tuple_count      = 0;
        h.free_space_start = static_cast<std::uint16_t>(SlottedPageHeader::kSize);
        h.free_space_end   = static_cast<std::uint16_t>(kBodySize);
        h.reserved         = 0;
        writeSpHeader(h);

        // Sync PageHeader fields
        page_.header().slot_count     = 0;
        page_.header().free_space_ptr = h.free_space_start;

        return Status::Ok;
    }

    // ── Mutation ──────────────────────────────────────────────────────────────

    Status SlottedPage::insertTuple(std::span<const std::byte> tuple,
                                    SlotId& slot_id) noexcept
    {
        if (tuple.empty())
        {
            return Status::InvalidArg;
        }

        SlottedPageHeader h = readSpHeader();
        const auto slot_count = page_.header().slot_count;
        const auto tuple_len  = static_cast<std::uint16_t>(tuple.size());

        // Check whether a deleted slot can be reused (avoids growing the dir)
        bool reuse = false;
        std::uint16_t reuse_idx = 0;
        for (std::uint16_t i = 0; i < slot_count; ++i)
        {
            TupleSlot s = readSlot(i);
            if (s.isDeleted())
            {
                reuse     = true;
                reuse_idx = i;
                break;
            }
        }

        // Bytes needed: tuple payload + (a new slot entry if we cannot reuse)
        const std::size_t needed_slot = reuse ? 0u : TupleSlot::kSize;
        const std::size_t needed_total = tuple.size() + needed_slot;
        const std::size_t available    = h.free_space_end - h.free_space_start;

        if (needed_total > available)
        {
            return Status::IoError; // page full
        }

        // Allocate tuple space from the high end of the free zone
        const auto new_free_end = static_cast<std::uint16_t>(h.free_space_end - tuple_len);

        // Copy tuple bytes into the body using Serializer — no memcpy in the
        // public-API path, no reinterpret_cast.
        auto body = page_.body();
        std::span<std::byte> dest(body.data() + new_free_end, tuple_len);
        Serializer ser(dest);
        static_cast<void>(ser.writeBytes(tuple));

        // Create or update the slot entry
        const TupleSlot new_slot(new_free_end, tuple_len);

        if (reuse)
        {
            slot_id = reuse_idx;
            writeSlot(reuse_idx, new_slot);
        }
        else
        {
            slot_id = slot_count;
            writeSlot(slot_count, new_slot);
            // Grow the slot directory
            h.free_space_start = static_cast<std::uint16_t>(
                h.free_space_start + TupleSlot::kSize);
            page_.header().slot_count = static_cast<std::uint16_t>(slot_count + 1);
        }

        h.free_space_end = new_free_end;
        h.tuple_count    = static_cast<std::uint16_t>(h.tuple_count + 1);
        writeSpHeader(h);
        page_.header().free_space_ptr = h.free_space_start;

        return Status::Ok;
    }

    Status SlottedPage::deleteTuple(SlotId slot_id) noexcept
    {
        const auto slot_count = page_.header().slot_count;
        if (slot_id >= slot_count)
        {
            return Status::NotFound;
        }

        TupleSlot slot = readSlot(slot_id);
        if (slot.isDeleted())
        {
            return Status::InvalidArg; // already deleted
        }

        slot.markDeleted();
        writeSlot(slot_id, slot);

        SlottedPageHeader h = readSpHeader();
        h.tuple_count = static_cast<std::uint16_t>(h.tuple_count - 1);
        writeSpHeader(h);

        return Status::Ok;
    }

    Status SlottedPage::compact(std::size_t& reclaimed_bytes) noexcept
    {
        const auto slot_count = page_.header().slot_count;
        SlottedPageHeader h   = readSpHeader();

        // Scratch buffer for the compacted tuple area — stack allocation only.
        // Maximum usable body is kBodySize bytes; we compact into a local buffer.
        std::array<std::byte, Page::kBodySize> scratch{};
        std::uint16_t write_ptr = static_cast<std::uint16_t>(kBodySize); // fills down

        // Walk slots in reverse order so high-offset tuples are placed first
        // (preserves the invariant that tuples pack contiguously at the top)
        for (std::uint16_t i = slot_count; i > 0; --i)
        {
            const std::uint16_t idx = static_cast<std::uint16_t>(i - 1u);
            TupleSlot slot = readSlot(idx);
            if (slot.isDeleted())
            {
                continue;
            }

            // Copy active tuple bytes into the scratch buffer
            const auto body = page_.body();
            write_ptr = static_cast<std::uint16_t>(write_ptr - slot.length);
            for (std::uint16_t b = 0; b < slot.length; ++b)
            {
                scratch[write_ptr + b] = body[slot.offset + b];
            }

            // Update the slot to point at the new position
            slot.offset = write_ptr;
            writeSlot(idx, slot);
        }

        // The bytes reclaimed equal the gap between the old freeSpaceEnd and
        // the new write pointer.
        const std::uint16_t old_end = h.free_space_end;
        reclaimed_bytes = (write_ptr >= old_end) ? static_cast<std::size_t>(write_ptr - old_end)
                                                  : 0u;

        // Write compacted tuples back into the body
        auto body = page_.body();
        for (std::size_t j = write_ptr; j < kBodySize; ++j)
        {
            body[j] = scratch[j];
        }

        // Update the slotted-page header
        h.free_space_end = write_ptr;
        writeSpHeader(h);

        return Status::Ok;
    }

    // ── Queries ───────────────────────────────────────────────────────────────

    Status SlottedPage::readTuple(SlotId slot_id,
                                  std::span<const std::byte>& tuple) const noexcept
    {
        const auto slot_count = page_.header().slot_count;
        if (slot_id >= slot_count)
        {
            return Status::NotFound;
        }

        const TupleSlot slot = readSlot(slot_id);
        if (slot.isDeleted())
        {
            return Status::InvalidArg;
        }

        auto body = page_.body();
        tuple = std::span<const std::byte>(body.data() + slot.offset, slot.length);
        return Status::Ok;
    }

    std::size_t SlottedPage::freeSpace() const noexcept
    {
        const SlottedPageHeader h = readSpHeader();
        return h.free_space_end > h.free_space_start
                   ? static_cast<std::size_t>(h.free_space_end - h.free_space_start)
                   : 0u;
    }

    std::uint16_t SlottedPage::slotCount() const noexcept
    {
        return page_.header().slot_count;
    }

    std::uint16_t SlottedPage::tupleCount() const noexcept
    {
        return readSpHeader().tuple_count;
    }

    bool SlottedPage::isFull(std::size_t tuple_size) const noexcept
    {
        // Even if a deleted slot exists (no slot-dir growth needed), we still
        // need space for the tuple payload.
        const std::size_t free = freeSpace();
        const auto slot_count  = page_.header().slot_count;

        // Check if any deleted slot can be reused
        bool has_deleted = false;
        for (std::uint16_t i = 0; i < slot_count; ++i)
        {
            if (readSlot(i).isDeleted())
            {
                has_deleted = true;
                break;
            }
        }

        const std::size_t needed = has_deleted ? tuple_size : tuple_size + TupleSlot::kSize;
        return free < needed;
    }

} // namespace hamdb
