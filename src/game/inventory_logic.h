#pragma once

#include <cstdint>

namespace trinity::game
{
    // A generated item is usable only when the operation can be committed to
    // a distinct authority holder before its client mirror is touched.
    bool CanCommitAuthoritativeAdd(bool primitivesReady, bool haveDefinition,
                                   uintptr_t clientHolder, uintptr_t serverHolder);

    bool ShouldRetryAuthoritativeAdd(bool primitivesReady, bool haveDefinition,
                                     uintptr_t clientHolder, uintptr_t serverHolder,
                                     int attempts, int maxAttempts);

    // Retry a missing catalog resolver at a bounded cadence. Once the table is
    // present the static catalog is built once and no further scans are needed.
    bool ShouldAttemptCatalogResolve(bool haveItemTableGlobal,
                                     uint64_t nowMs, uint64_t lastAttemptMs,
                                     uint64_t retryIntervalMs);
}
