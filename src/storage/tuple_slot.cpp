#include "storage/tuple_slot.hpp"

namespace hamdb
{

    TupleSlot::TupleSlot(std::uint16_t off, std::uint16_t len) noexcept
        : offset(off), length(len), flags(0), reserved(0)
    {
    }

    bool TupleSlot::isDeleted() const noexcept
    {
        return (flags & kFlagDeleted) != 0u;
    }

    void TupleSlot::markDeleted() noexcept
    {
        flags |= kFlagDeleted;
    }

    void TupleSlot::clearDeleted() noexcept
    {
        flags &= static_cast<std::uint16_t>(~kFlagDeleted);
    }

} // namespace hamdb
