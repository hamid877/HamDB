#include "common/enums.hpp"

namespace hamdb
{

    const char* statusToString(Status status) noexcept
    {
        switch (status)
        {
        case Status::Ok:
            return "Ok";
        case Status::NotFound:
            return "NotFound";
        case Status::InvalidArg:
            return "InvalidArg";
        case Status::IoError:
            return "IoError";
        case Status::OutOfMemory:
            return "OutOfMemory";
        case Status::Corruption:
            return "Corruption";
        case Status::NotSupported:
            return "NotSupported";
        case Status::AlreadyExists:
            return "AlreadyExists";
        case Status::BufferPoolFull:
            return "BufferPoolFull";
        case Status::Unknown:
        default:
            return "Unknown";
        }
    }

} // namespace hamdb
