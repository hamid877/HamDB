#include "storage/page_header.h"

namespace hamdb {

PageHeader::PageHeader(PageId id, PageType type)
    : page_id(id)
    , page_type(type)
    , free_space_ptr(static_cast<std::uint16_t>(kSize))
    , slot_count(0)
    , checksum(0)
{}

bool PageHeader::operator==(const PageHeader& other) const noexcept {
    return page_id        == other.page_id
        && page_type      == other.page_type
        && free_space_ptr == other.free_space_ptr
        && slot_count     == other.slot_count
        && checksum       == other.checksum;
}

bool PageHeader::operator!=(const PageHeader& other) const noexcept {
    return !(*this == other);
}

} // namespace hamdb
