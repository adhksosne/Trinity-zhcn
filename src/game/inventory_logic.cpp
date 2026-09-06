#include "inventory_logic.h"

namespace trinity::game
{
    bool CanCommitAuthoritativeAdd(bool primitivesReady, bool haveDefinition,
                                   uintptr_t clientHolder, uintptr_t serverHolder)
    {
        return primitivesReady && haveDefinition && clientHolder != 0 &&
               serverHolder != 0 && serverHolder != clientHolder;
    }

    bool ShouldRetryAuthoritativeAdd(bool primitivesReady, bool haveDefinition,
                                     uintptr_t clientHolder, uintptr_t serverHolder,
                                     int attempts, int maxAttempts)
    {
        return primitivesReady && haveDefinition && clientHolder != 0 &&
               serverHolder == 0 && attempts >= 0 && attempts < maxAttempts;
    }

    int PreferPartyCharacterIndex(int partyIndex, int gearIdentity)
    {
        if (partyIndex >= 0 && partyIndex <= 2) return partyIndex;
        return (gearIdentity >= 0 && gearIdentity <= 2) ? gearIdentity : -1;
    }
}
