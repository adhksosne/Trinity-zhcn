#pragma once

#include <cstdint>

namespace trinity::core
{
    // Returns the modern Crimson Desert title-update label encoded by the PE
    // revision, or nullptr when the revision predates the 2.00 update family.
    const char* ModernTitleUpdateForRevision(uint16_t revision);
}
