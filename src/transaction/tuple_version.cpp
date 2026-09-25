#include "transaction/tuple_version.hpp"

// TupleVersion is a plain aggregate; all logic lives in the header.
// This translation unit exists to satisfy the project's header/source
// separation requirement and to give the linker a definition unit.

namespace hamdb
{
    // Nothing to define here — TupleVersion members are all inline.
} // namespace hamdb
