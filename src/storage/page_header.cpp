#include "storage/page_header.hpp"

namespace hamdb
{

    PageHeader::PageHeader(PageId page_id, PageType page_type)
        : page_id(page_id),
          page_type(page_type)
    {
    }

    bool PageHeader::operator==(const PageHeader& other) const noexcept
    {
        return page_id == other.page_id && page_type == other.page_type &&
               free_space_ptr == other.free_space_ptr && slot_count == other.slot_count &&
               checksum == other.checksum;
    }

    bool PageHeader::operator!=(const PageHeader& other) const noexcept
    {
        return !(*this == other);
    }

} // namespace hamdb
