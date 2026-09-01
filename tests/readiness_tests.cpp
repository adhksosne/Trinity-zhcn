#include "../src/core/readiness.h"
#include "../src/core/version_mapping.h"
#include "../src/mem/section_filter.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    int failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }

    void DelayedCodeIsRetriedUntilReady()
    {
        uint64_t now = 0;
        int probes = 0;
        int pauses = 0;

        const bool ready = trinity::core::WaitForReadiness(
            [&] { return ++probes == 3; },
            [&] { return now; },
            [&](uint32_t ms) { now += ms; ++pauses; },
            5000, 250);

        Expect(ready, "delayed code should become ready");
        Expect(probes == 3, "readiness must be probed until the third successful attempt");
        Expect(pauses == 2, "the wait must pause only between failed probes");
        Expect(now == 500, "retry interval must be applied exactly");
    }

    void MissingCodeTimesOutWithoutAnExtraFullSleep()
    {
        uint64_t now = 0;
        int probes = 0;
        int pauses = 0;

        const bool ready = trinity::core::WaitForReadiness(
            [&] { ++probes; return false; },
            [&] { return now; },
            [&](uint32_t ms) { now += ms; ++pauses; },
            600, 250);

        Expect(!ready, "permanently missing code must time out");
        Expect(probes == 4, "timeout boundary must receive one final readiness probe");
        Expect(pauses == 3, "timeout should use two full pauses and one bounded remainder");
        Expect(now == 600, "the final pause must be clamped to the remaining timeout");
    }

    void PeRevisionMapsToCurrentTitleUpdate()
    {
        using trinity::core::ModernTitleUpdateForRevision;

        Expect(std::strcmp(ModernTitleUpdateForRevision(2692), "2.00.02") == 0,
               "PE revision 2692 must identify TU 2.00.02");
        Expect(std::strcmp(ModernTitleUpdateForRevision(2658), "2.00.01") == 0,
               "PE revision 2658 must remain TU 2.00.01");
        Expect(std::strcmp(ModernTitleUpdateForRevision(2625), "2.00.00") == 0,
               "PE revision 2625 must remain TU 2.00.00");
        Expect(ModernTitleUpdateForRevision(2474) == nullptr,
               "pre-2.00 revisions must keep using binary fingerprinting");
    }

    void ExecutableDebugSectionIsScanned()
    {
        using trinity::mem::ShouldScanSection;

        Expect(ShouldScanSection(".debug", 0x20000000u),
               "TU 2.00.02 executable .debug section must be scanned");
        Expect(!ShouldScanSection(".debug", 0x40000000u),
               "non-executable stale .debug data should remain excluded");
        Expect(ShouldScanSection(".text", 0x60000020u),
               "normal executable sections must remain scannable");
    }
}

int main()
{
    DelayedCodeIsRetriedUntilReady();
    MissingCodeTimesOutWithoutAnExtraFullSleep();
    PeRevisionMapsToCurrentTitleUpdate();
    ExecutableDebugSectionIsScanned();
    if (failures == 0)
        std::puts("readiness tests passed");
    return failures == 0 ? 0 : 1;
}
