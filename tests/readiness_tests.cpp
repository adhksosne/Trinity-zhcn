#include "../src/core/readiness.h"
#include "../src/core/version_mapping.h"
#include "../src/game/crime_hook_contract.h"
#include "../src/game/inventory_hook_contract.h"
#include "../src/mem/section_filter.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

static_assert(std::is_same_v<trinity::game::RegisterCrimeEvent_t,
                             void(__fastcall*)(void*, const char*, void*, void*)>,
              "crime-event forwarding must preserve the full 64-bit string pointer");
static_assert(std::is_same_v<trinity::game::InventoryCommit201_t,
                             void*(__fastcall*)(void*, void*, void*, uint16_t,
                                                void*, uint8_t, uint8_t, uint8_t)>,
              "TU 2.01 inventory commit hook must forward all eight arguments");

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

        Expect(std::strcmp(ModernTitleUpdateForRevision(2760), "2.01.00") == 0,
               "PE revision 2760 must identify TU 2.01.00");
        Expect(std::strcmp(ModernTitleUpdateForRevision(2692), "2.00.02") == 0,
               "PE revision 2692 must identify TU 2.00.02");
        Expect(std::strcmp(ModernTitleUpdateForRevision(2658), "2.00.01") == 0,
               "PE revision 2658 must remain TU 2.00.01");
        Expect(std::strcmp(ModernTitleUpdateForRevision(2625), "2.00.00") == 0,
               "PE revision 2625 must remain TU 2.00.00");
        Expect(ModernTitleUpdateForRevision(2761) == nullptr,
               "an unrecognised newer PE revision must not be labelled as a known title update");
        Expect(ModernTitleUpdateForRevision(2474) == nullptr,
               "pre-2.00 revisions must keep using binary fingerprinting");
    }

    void CurrentUpdateUsesCompatibleReadinessProfile()
    {
        using trinity::core::ReadinessProfile;
        using trinity::core::ReadinessProfileForRevision;

        Expect(ReadinessProfileForRevision(2760) == ReadinessProfile::Tu201KnownCompatible,
               "PE revision 2760 must not wait for removed TU 2.00.02 signatures");
        Expect(ReadinessProfileForRevision(2692) == ReadinessProfile::LegacyComplete,
               "PE revision 2692 must retain the complete TU 2.00.02 readiness profile");
    }

    void MovementOwnerOffsetTracksCurrentLayout()
    {
        using trinity::core::MoveComponentOwnerOffsetForRevision;

        Expect(MoveComponentOwnerOffsetForRevision(2760) == 0x2B8,
               "PE revision 2760 locomotion component must use move-owner offset 0x2B8");
        Expect(MoveComponentOwnerOffsetForRevision(2692) == 0x298,
               "pre-2.01 locomotion components must retain move-owner offset 0x298");
    }

    void CurrentUpdateRejectsLegacyFuzzySignatures()
    {
        using trinity::core::MayUseLegacyFuzzySignaturesForRevision;

        Expect(!MayUseLegacyFuzzySignaturesForRevision(2760),
               "PE revision 2760 must not resolve native calls through broad legacy signatures");
        Expect(!MayUseLegacyFuzzySignaturesForRevision(2761),
               "an unknown newer revision must fail closed instead of using legacy fuzzy signatures");
        Expect(MayUseLegacyFuzzySignaturesForRevision(2692),
               "TU 2.00.02 must retain its legacy compatibility fallbacks");
    }

    void InventoryRootAnchorTracksCurrentInstructionLayout()
    {
        using trinity::core::InventoryCoreGlobalMovOffsetForRevision;

        Expect(InventoryCoreGlobalMovOffsetForRevision(2760) == 0,
               "TU 2.01 inventory-root signature must resolve RIP at the match start");
        Expect(InventoryCoreGlobalMovOffsetForRevision(2692) == 0x15,
               "pre-2.01 inventory-root signature must retain its legacy mov offset");
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
    CurrentUpdateUsesCompatibleReadinessProfile();
    MovementOwnerOffsetTracksCurrentLayout();
    CurrentUpdateRejectsLegacyFuzzySignatures();
    InventoryRootAnchorTracksCurrentInstructionLayout();
    ExecutableDebugSectionIsScanned();
    if (failures == 0)
        std::puts("readiness tests passed");
    return failures == 0 ? 0 : 1;
}
