#include "storage/tuple.hpp"

namespace hamdb
{

    Tuple::Tuple(std::span<const std::byte> data)
        : data_(data.begin(), data.end())
    {
    }

    std::span<const std::byte> Tuple::data() const noexcept
    {
        return data_;
    }

    std::size_t Tuple::size() const noexcept
    {
        return data_.size();
    }

    bool Tuple::empty() const noexcept
    {
        return data_.empty();
    }

} // namespace hamdb
