#include "readiness.h"

#include <algorithm>

namespace trinity::core
{
    bool WaitForReadiness(const ReadinessProbe& probe,
                          const ReadinessClock& now,
                          const ReadinessPause& pause,
                          uint32_t timeoutMs,
                          uint32_t intervalMs)
    {
        if (!probe || !now || !pause)
            return false;

        const uint64_t start = now();
        for (;;)
        {
            if (probe())
                return true;

            const uint64_t elapsed = now() - start;
            if (elapsed >= timeoutMs)
                return false;

            const uint64_t remaining = timeoutMs - elapsed;
            const uint32_t delay = static_cast<uint32_t>(
                std::min<uint64_t>(intervalMs ? intervalMs : 1, remaining));
            pause(delay);
        }
    }
}
