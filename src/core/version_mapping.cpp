#include "version_mapping.h"

namespace trinity::core
{
    const char* ModernTitleUpdateForRevision(uint16_t revision)
    {
        switch (revision)
        {
        case 2760: return "2.01.00";
        case 2692: return "2.00.02";
        case 2658: return "2.00.01";
        case 2625: return "2.00.00";
        default:   return nullptr;
        }
    }

    uintptr_t MoveComponentOwnerOffsetForRevision(uint16_t revision)
    {
        return revision == 2760 ? 0x2B8 : 0x298;
    }

    bool MayUseLegacyFuzzySignaturesForRevision(uint16_t revision)
    {
        return revision < 2760;
    }

    uintptr_t InventoryCoreGlobalMovOffsetForRevision(uint16_t revision)
    {
        return revision == 2760 ? 0 : 0x15;
    }

    uintptr_t RealmFlagOffsetForRevision(uint16_t revision)
    {
        return revision == 2760 ? 0x1FD : 0x1F2;
    }
}
