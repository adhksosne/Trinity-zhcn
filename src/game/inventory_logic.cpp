#include "inventory_logic.h"

namespace trinity::game
{
    bool CanCommitAuthoritativeAdd(bool primitivesReady, bool haveDefinition,
                                   uintptr_t clientHolder, uintptr_t serverHolder)
    {
        return primitivesReady && haveDefinition && clientHolder != 0 &&
               serverHolder != 0 && serverHolder != clientHolder;
    }
}
