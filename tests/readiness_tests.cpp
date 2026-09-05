#include "../src/core/readiness.h"
#include "../src/core/version_mapping.h"
#include "../src/game/crime_hook_contract.h"
#include "../src/game/inventory_hook_contract.h"
#include "../src/game/inventory_logic.h"
#include "../src/game/player_logic.h"
#include "../src/game/equipment_logic.h"
#include "../src/game/offsets.h"
#include "../src/mem/section_filter.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

namespace trinity::core
{
    uintptr_t RealmFlagOffsetForRevision(uint16_t revision);
}

namespace trinity::game
{
    int64_t ScaleTrustValue(int64_t previous, bool hasPrevious,
                            int64_t incoming, float multiplier, bool enabled);
    bool SelectTrustBaseline(int64_t stored, bool hasStored,
                             int64_t cached, bool hasCached,
                             int64_t* outBaseline);
}

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

    void RealmFlagOffsetTracksCurrentTlsLayout()
    {
        using trinity::core::RealmFlagOffsetForRevision;

        Expect(RealmFlagOffsetForRevision(2760) == 0x1FD,
               "TU 2.01 realm selection must use the new TLS byte at +0x1FD");
        Expect(RealmFlagOffsetForRevision(2692) == 0x1F2,
               "pre-2.01 builds must retain the legacy TLS byte at +0x1F2");
    }

    void TrustScalingUsesTheFirstPositiveGain()
    {
        using trinity::game::ScaleTrustValue;

        Expect(ScaleTrustValue(0, false, 5, 25.0f, true) == 100,
               "a first +5 trust event at x25 must immediately reach 100");
        Expect(ScaleTrustValue(20, true, 25, 3.0f, true) == 35,
               "an existing +5 trust event at x3 must add 15 to the old value");
        Expect(ScaleTrustValue(20, true, 15, 25.0f, true) == 15,
               "trust losses must pass through without multiplication");
        Expect(ScaleTrustValue(20, true, 25, 25.0f, false) == 25,
               "disabled trust scaling must leave the incoming value unchanged");
    }

    void CachedTrustBaselineWinsOverAliasedLiveRecord()
    {
        using trinity::game::SelectTrustBaseline;

        int64_t baseline = -1;
        Expect(SelectTrustBaseline(5, true, 0, true, &baseline) && baseline == 0,
               "cached pre-write trust must win when the live source aliases the destination");
        Expect(SelectTrustBaseline(20, true, 0, false, &baseline) && baseline == 20,
               "live-map trust must seed the cache when no prior observation exists");
        Expect(!SelectTrustBaseline(0, false, 0, false, &baseline),
               "missing cache and live-map state must report no baseline");
    }

    void AddItemRequiresAnAuthoritativeServerHolder()
    {
        using trinity::game::CanCommitAuthoritativeAdd;

        Expect(!CanCommitAuthoritativeAdd(true, true, 0x1000, 0),
               "client-only add must fail closed instead of creating a ghost item");
        Expect(!CanCommitAuthoritativeAdd(true, true, 0x1000, 0x1000),
               "one holder cannot stand in for both client and server authority");
        Expect(CanCommitAuthoritativeAdd(true, true, 0x1000, 0x2000),
               "distinct client and server holders may commit an authoritative add");
    }

    void AddItemRetriesOnlyWhileAuthorityIsMissing()
    {
        using trinity::game::ShouldRetryAuthoritativeAdd;

        Expect(ShouldRetryAuthoritativeAdd(true, true, 0x1000, 0, 0, 120),
               "a ready add may wait for the server holder");
        Expect(!ShouldRetryAuthoritativeAdd(true, true, 0x1000, 0, 120, 120),
               "an authority wait must stop at the retry limit");
        Expect(!ShouldRetryAuthoritativeAdd(true, true, 0x1000, 0x2000, 0, 120),
               "a complete authority pair must commit instead of retrying");
        Expect(!ShouldRetryAuthoritativeAdd(false, true, 0x1000, 0, 0, 120),
               "an incomplete engine path must fail instead of retrying forever");
    }

    void GodModeRequiresStrictPlayerTarget()
    {
        using trinity::game::ShouldBlockPlayerDamage;

        Expect(ShouldBlockPlayerDamage(false, true, false) == false,
               "God Mode off must never block damage");
        Expect(ShouldBlockPlayerDamage(true, false, false) == false,
               "God Mode must not block damage to an unclassified enemy");
        Expect(ShouldBlockPlayerDamage(true, true, false),
               "God Mode blocks damage to a strict player target");
        Expect(ShouldBlockPlayerDamage(true, false, true),
               "God Mode blocks damage to a tracked mount");
    }

    void PartyIndexWinsForSelectedEquipmentComponent()
    {
        using trinity::game::AcceptCharacterComponent;

        Expect(!AcceptCharacterComponent(2, 1, 2),
               "a Damiane gear identity must not be routed to the selected Oongka");
        Expect(AcceptCharacterComponent(2, 2, 1),
               "a matching Oongka gear identity must win over party position");
        Expect(AcceptCharacterComponent(2, -1, 2),
               "an unidentified selected party actor remains usable");
        Expect(!AcceptCharacterComponent(2, 1, 1),
               "a different party actor cannot be used for Oongka");
    }

    void PartyContainerIdentityWinsOverStaleGearIdentity()
    {
        using trinity::game::PreferPartyCharacterIndex;

        Expect(PreferPartyCharacterIndex(2, 0) == 2,
               "the active Oongka container must not fall back to stale Kliff gear");
        Expect(PreferPartyCharacterIndex(-1, 1) == 1,
               "gear identity remains a fallback when party order is unavailable");
        Expect(PreferPartyCharacterIndex(-1, -1) == -1,
               "unknown character identity must remain unknown");
    }

    void TrustRecordUsesTheCopiedValueField()
    {
        Expect(trinity::game::kOff_FriendlyRec_Value == 0x20,
               "TU 2.01 trust must read the value copied from record+0x20");
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
    RealmFlagOffsetTracksCurrentTlsLayout();
    TrustScalingUsesTheFirstPositiveGain();
    CachedTrustBaselineWinsOverAliasedLiveRecord();
    AddItemRequiresAnAuthoritativeServerHolder();
    AddItemRetriesOnlyWhileAuthorityIsMissing();
    GodModeRequiresStrictPlayerTarget();
    PartyIndexWinsForSelectedEquipmentComponent();
    PartyContainerIdentityWinsOverStaleGearIdentity();
    TrustRecordUsesTheCopiedValueField();
    ExecutableDebugSectionIsScanned();
    if (failures == 0)
        std::puts("readiness tests passed");
    return failures == 0 ? 0 : 1;
}
