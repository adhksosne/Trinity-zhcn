#include "version_mapping.h"

namespace trinity::core
{
    const char* ModernTitleUpdateForRevision(uint16_t revision)
    {
        if (revision >= 2692)
            return "2.00.02";
        if (revision >= 2650)
            return "2.00.01";
        if (revision >= 2625)
            return "2.00.00";
        return nullptr;
    }
}
