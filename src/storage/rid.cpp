#include "storage/rid.hpp"

namespace hamdb
{

    RID::RID(PageId page_id, std::uint16_t slot_id)
        : page_id_(page_id), slot_id_(slot_id)
    {
    }

    PageId RID::getPageId() const noexcept
    {
        return page_id_;
    }

    std::uint16_t RID::getSlotId() const noexcept
    {
        return slot_id_;
    }

    bool RID::isValid() const noexcept
    {
        return page_id_ != kInvalidPageId;
    }

    bool RID::operator==(const RID& other) const noexcept
    {
        return page_id_ == other.page_id_ && slot_id_ == other.slot_id_;
    }

    bool RID::operator!=(const RID& other) const noexcept
    {
        return !(*this == other);
    }

    bool RID::operator<(const RID& other) const noexcept
    {
        if (page_id_ != other.page_id_)
        {
            return page_id_ < other.page_id_;
        }
        return slot_id_ < other.slot_id_;
    }

    bool RID::operator>(const RID& other) const noexcept
    {
        return other < *this;
    }

    bool RID::operator<=(const RID& other) const noexcept
    {
        return !(other < *this);
    }

    bool RID::operator>=(const RID& other) const noexcept
    {
        return !(*this < other);
    }

} // namespace hamdb
