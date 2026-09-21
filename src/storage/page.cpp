#include "storage/page.hpp"
#include <cstring>

namespace hamdb
{

    Page::Page() : data_{} {}

    Page::Page(const PageHeader& header) : data_{}, header_(header) {}

    PageHeader& Page::header()
    {
        return header_;
    }

    const PageHeader& Page::header() const
    {
        return header_;
    }

    std::span<std::byte> Page::data()
    {
        return std::span<std::byte>(data_);
    }

    std::span<const std::byte> Page::data() const
    {
        return std::span<const std::byte>(data_);
    }

    std::span<std::byte> Page::body()
    {
        return std::span<std::byte>(data_).subspan(PageHeader::kSize);
    }

    std::span<const std::byte> Page::body() const
    {
        return std::span<const std::byte>(data_).subspan(PageHeader::kSize);
    }

    void Page::clear()
    {
        data_.fill(std::byte{0});
        header_ = PageHeader{};
    }

    PageId Page::id() const
    {
        return header_.page_id;
    }

} // namespace hamdb
