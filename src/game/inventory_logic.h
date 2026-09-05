#pragma once

#include <cstdint>

namespace trinity::game
{
    // A generated item is usable only when the operation can be committed to
    // a distinct authority holder before its client mirror is touched.
    bool CanCommitAuthoritativeAdd(bool primitivesReady, bool haveDefinition,
                                   uintptr_t clientHolder, uintptr_t serverHolder);
}
