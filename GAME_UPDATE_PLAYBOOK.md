# Trinity: game update maintenance playbook

Start here when Crimson Desert updates and Trinity features stop working.
This is an agent-facing navigation and verification guide, not a claim that
every feature works on the next game build.

Source baseline inspected: 2026-09-06, commit `34d1c50` (inventory limit 1999).
The current revision map includes PE revision 2760 -> TU 2.01.00. Every address,
layout, signature and revision-specific branch must be revalidated for a new EXE.
This document was prepared from source inspection and historical repair notes;
no game execution or live feature validation was performed when creating it.

## 1. First ten minutes

1. Read this file, then inspect `git status --short` and the current commit.
   Preserve unrelated edits. Follow any applicable `AGENTS.md` instructions.
2. Establish the actual game executable path, PID, PE version, SHA-256, module
   base and image size. TU marketing version and PE file version are different.
   Do not assume the installation path, process base or an old dump is current.
3. Identify the loaded ASI path and compare its hash with the intended artifact.
   Read the startup version/build line and failures in `Trinity.log` if logging
   is enabled. A file on disk does not prove the process loaded that same build.
4. If Cheat Engine MCP is available, confirm connectivity, attached PID and module
   inventory. Start with read-only memory, scans and disassembly. For an
   investigative request, establish the cause before editing; perform fixes and
   live mutations within the user's authorized scope.
5. Group failures: overlay/bootstrap, widespread missing signatures, a shared
   player/realm dependency, or one feature. Fix shared dependencies first.
6. Record evidence for each candidate before changing it: match count, RVA,
   instruction boundary, callers, argument layout and the observed game action.

Useful read-only commands, from the repository root in PowerShell:

```powershell
git status --short
git log -5 --oneline
rg -n 'kSig_|kCharMgrAnchors|kOff_|kTls_' src/game/offsets.h
rg -n 'FindPattern|FindAllMatches|CountMatches|InstallHook' src/game src/core
rg -n '2760|2692|revision|Pre201|Legacy|0x1FD|0x1F2' src tests
rg -n 'Readiness|WaitForReadiness|ShouldScanSection' src tests
rg -n 'LOC\(|State::Get|MarkerStatus|ConsumeMarkerResult' src/gui/menu.cpp
```

For the confirmed executable path, use `Get-Item` / `.VersionInfo` and
`Get-FileHash -Algorithm SHA256`; do not hash a similarly named stale copy.

## 2. Source navigation and dependency map

| Area | Start here | What to inspect after a patch |
| --- | --- | --- |
| Startup / readiness | `src/dllmain.cpp`, `src/core/mod.cpp`, `src/core/readiness.*` | Initialization order, representative readiness signatures, timeout/profile selection, installer results |
| Version policy | `src/core/version_detect.*`, `src/core/version_mapping.*` | PE/TU mapping, movement-owner offset, TLS realm offset, inventory RIP-load offset, legacy fuzzy policy |
| Scanner / hooks | `src/mem/scanner.*`, `src/mem/section_filter.*`, `src/mem/hooks.h` | PE section flags, readable regions, uniqueness, RIP resolution, valid hook boundaries |
| Layout registry | `src/game/offsets.h` | AOBs, alternate signatures, member offsets, ABI notes and original reverse-engineering anchors |
| Player / combat | `src/game/player.cpp`, `src/game/player_logic.*` | Character-manager consensus, controlled-body identity, stat pools, damage source/target routing |
| Movement / teleport | `src/game/teleport.cpp`, `src/game/map_marker.h`, `src/game/marker_teleport_logic.h` | Movement and locomotion hooks, node tables, waypoint state, write order and result confirmation |
| Inventory / crime | `src/game/inventory.cpp`, `src/game/inventory_logic.*`, `src/game/inventory_hook_contract.h`, `src/game/crime_hook_contract.h` | Holder identity, transaction ABI, dual-realm commit, slots/stacks, wanted-state routing |
| Trust | `src/game/friendly.cpp`, `src/game/friendly_logic.*` | NPC/pet setters and lookups, trust record layout, pre-write baseline and delta scaling |
| World | `src/game/world.cpp` | Simulation timer, field time, visible sun, weather hooks and environment manager |
| Equipment | `src/game/equipment.cpp`, `src/game/equipment_logic.*` | Selected character/item instance, sockets, refine/durability/stat changes, effect refresh |
| Dye / appearance | `src/game/dye.cpp`, `src/game/dye_data.h`, `src/game/dye_slots_table.h` | Equip capture, channels, apply/upsert/remove functions, render update and durable item records |
| Catalog / assets | `src/game/item_names.*`, `src/game/pak.*`, `src/gui/icons.*` | Item tables, localized names, archive layout, icon lookup; distinguish data failure from hook failure |
| UI / persistence / input | `src/gui/menu.cpp`, `src/gui/framework.cpp`, `src/core/state.h`, `src/core/settings.cpp`, `src/core/localization.cpp`, `src/hooks/input.cpp`, `src/hooks/xinput_hook.cpp` | Actual exposed actions, settings, hotkeys, controller bindings and result messages |
| Renderer | `src/hooks/dx12_hook.cpp`, `src/hooks/hdr_composite_shader.h` | Present/render path, resize, HDR and overlay input when gameplay hooks are healthy |

`offsets.h` is the main registry, but it is NOT the complete dependency list.
For example, `map_marker.h` contains literal offsets, `teleport.cpp` contains
the current waypoint signature, and installers select revision-specific ABIs.
Search all consumers before changing a shared definition.

## 3. Re-find functions without guessing

### Widespread NOT FOUND

Check `ShouldScanSection` before replacing signatures. A historical TU 2.00.02
failure involved executable gameplay code in `.debug`; filtering by section name
alone hid valid code. Preserve permission/region checks and exclusion of non-code
debug data. The on-disk image and the initialized live image can differ.

Check delayed code availability next. `GameplayCodeReady` uses different sentinel
sets for known TU 2.01 and older builds. Readiness presence is not uniqueness or
full feature coverage. A stale sentinel can force the entire timeout. Confirm
that a proposed sentinel actually exists in the new build before updating the
profile; do not simply shorten the wait or remove every failing requirement.

### Individual missing or ambiguous signatures

1. Start at the named `kSig_*` declaration and find every consumer with `rg`.
2. Use old disassembly/comments as search clues: RTTI/reflection names, table
   references, field access patterns, call relationships and the operation's data
   flow. Raw historical VAs are not new hook targets.
3. Find the equivalent routine in the new initialized image. Determine whether
   it is a writer, accessor, dispatcher, thunk or inlined replacement.
4. Verify function boundaries, Win64 register/stack arguments, return type,
   overwrite length and original-call behavior before installing a detour.
5. Build an AOB around stable semantics. Wildcard relocation/immediate bytes only
   when justified. Count matches across the same regions the runtime scans.
   Do not assume one match implies the correct semantic target.
6. For globals, resolve RIP-relative loads using the actual instruction length
   and displacement location. Validate indirection depth and object contents.
7. Revalidate structure members and calling contracts even if the AOB survives.
   Change declaration, installer, version policy and consumers together as needed.

Character-manager anchors intentionally include a multi-match fallback whose
sites should agree on one global. Preserve this consensus contract instead of
blindly demanding one raw match or accepting a different realm's manager.

## 4. Critical contracts from previous repairs

### Controlled player and stats

Follow `kCharMgrAnchors` through the character list and the controlled-body
predicate. The old `owner + 0x48` object-type word is not a reliable local-player
selector. Inspect type-descriptor and possessor round-trip checks in the current
implementation, especially after mounting, changing character or loading.

Primary anchors include `kSig_StatCommit`, `kSig_DamageApply`,
`kSig_CombatTimingEval` and `kSig_JustCore`, plus their alternatives. Different
stat entries can look like stamina/spirit while only one governs actual use.
Observe the meter during the relevant action; a full secondary field is not proof.

### Revision switches are executable behavior

At this source baseline, revision 2760 selects movement-owner offset `0x2B8`
instead of `0x298`, and TLS realm offset `0x1FD` instead of `0x1F2` through
`version_mapping.cpp`. Inventory also chooses transaction/placement ABIs by
revision. Unknown future revisions can fall through older defaults. Adding only
a display-name mapping is insufficient; audit every revision branch and caller.

### Add Item, slots and stacks

Re-find `kSig_InvGetHolder`, `kSig_InvCoreGlobal`, `kSig_TrItemValueCtor`,
`kSig_InvCommit`, `kSig_InvHolderInsert201` and placement helpers as a connected
transaction path. Preserve the captured authoritative holder argument and its
container; plausible client inventory memory is not an authoritative substitute.

Require distinct valid server and client holders and the intended realm during
each engine call. Preserve server-first commit and rejection of client-only or
same-holder operations. A useful diagnostic is `server=1 client=1`; additionally
check that the item can be used/equipped and survives the appropriate reload.

Slot size uses `kSig_InvSetExpandSlots` and expansion semantics, not an arbitrary
table-cap write. The baseline UI limit is `kMaxInventorySlots = 1999`; read the
current shared constant and validate the engine's actual capacity behavior.
Preserve non-stackable/unique item handling. Historical forced stack overrides
caused server validation error `298648703`.

### Trust

Use `kSig_FriendlySetNpc201`, `kSig_FriendlySetPet201` and their Get counterparts
to locate the current path. Historical TU 2.01 evidence described `0x68` records
with trust at `+0x28`; verify against the new writer and lookup layout.
Preserve `SelectTrustBaseline` and `ScaleTrustValue` behavior when source and
destination alias. Test a real positive trust change for NPC and pet separately.
Historical notes did not establish complete live trust correctness.

### Right-click map waypoint and teleport

Inspect the signature in `teleport.cpp`, then `ReadCurrentMapMarker` in
`map_marker.h`: current source reads global -> UI state -> destination at `+0xA8`,
then XYZ at destination `+0x20/+0x24/+0x28`. Re-find this using an actual right-click
waypoint that changes position; custom pins and captured hooks may follow other
paths. Historical `hooks=5/5` did not prove capture of this action.

Preserve finite/range/pointer validation. All-zero coordinates are rejected;
zero altitude by itself is valid. `MarkerStatus::Queued` means only queued.
The movement hook applies after `oMoveUpdate`; the current helper writes position
at `+0x90` and `+0x1A0`, clears velocity at `+0xC0/+0xD0`, and checks readback within
`0.5f`. The pending flow allows three attempts. Confirm the new movement layout
before reusing these values. Only confirmed application should produce success
and the corresponding marker/protection side effects. Readback still needs a
visible in-game check for later correction or snap-back.

### Equipment, dye and world

Equipment and dye share equip capture and inventory identity. Check the selected
character/mount, slot tag and instance ID before following refresh/apply functions.
For dye, inspect both visual leaves and persistent inventory records: a color
visible once is not a persistence test. Use equip-batch / dye-upsert callers as
anchors if prologues change.

For time, distinguish simulation speed, field clock and the visible sun. Check
`kSig_FrameTimerBody`, `kSig_FieldTimeTick`, `kSig_TodEngineGlobal` and their
fallbacks. Weather uses rain/snow/dust/wind hooks plus the environment manager;
validate each independently and confirm normal behavior returns when disabled.

## 5. Feature validation ledger

Create one row per actual exposed action from `src/gui/menu.cpp`, including
one-shot buttons that are absent from `State`. The groups below are a checklist,
not a frozen count of features. Include any new or conditional features present
in the checkout/build being repaired.

| Group | Required runtime scenarios |
| --- | --- |
| Player / combat | God Mode, one-hit kill, incoming/outgoing multipliers, durability, fall damage, stamina on foot/mount, spirit, parry and evade; check target discrimination and toggle-off behavior |
| Movement | Super Run, Super Jump, Free Flight up/down bindings; grounded/airborne and mount/character transitions |
| Teleport | Right-click marker moved several times, missing marker, saved locations and exposed node/travel actions; verify actual destination and no snap-back |
| Inventory | Catalog/search, quantity changes, Add Item, bulk/collection actions exposed in UI, slots and stacks; normal pickups/vendor purchases, unique items and persistence |
| Crime / trust | No Bounty and normal crime behavior after disabling; actual NPC/pet trust deltas with multiplier on/off |
| Equipment | Exposed socket/gear, refinement, durability and stat operations; correct instance/character, effect refresh and persistence |
| Appearance | Exposed dye/material/condition/reset actions; supported characters/mounts, immediate render, re-equip and reload |
| World | Game speed, time advance/freeze, sun, presets, clouds/fog, rain/snow/dust/wind; restore normal behavior |
| System | Menu keyboard/controller opening, hotkeys, text capture, localization, settings save/load, resize/HDR where relevant |

Track each action as `not tested`, `static checked`, `built`, `live passed`,
`live failed` or `blocked`, with the exact test and EXE/ASI hashes. An installed
hook, a passing test suite or a success toast cannot mark an entire group passed.

## 6. Build, tests and deployment provenance

Read `CMakeLists.txt` and the build script before running them. At this baseline,
`Build_Trinity.ps1` also rewrites the build timestamp and creates/copies release
packages; it is not a compile-only command. `tools/deploy_master.ps1` should also
be reviewed before use. For a narrow repair, prefer direct CMake in a Visual
Studio x64 developer environment with CMake and Ninja available:

```powershell
cmake -S . -B build-update -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DENABLE_EXTENDED_HOOKS=OFF
cmake --build build-update --target Trinity TrinityReadinessTests TrinityMapMarkerTests
ctest --test-dir build-update --output-on-failure
git diff --check
```

Stop if any command fails. Select the extended-hooks option to match the
authorized target variant; verify that its sources exist before enabling it.
Do not reuse a build directory configured for a different generator/toolchain.
If CMake is absent from PATH, locate the installed VS CMake/`vcvars64.bat` as
`Build_Trinity.ps1` does; an existing `CMakeCache.txt` provides a local hint,
not a portable path guarantee.

Current CTest targets are `TrinityReadinessTests` (readiness, version, scanner
and extracted gameplay logic contracts) and `TrinityMapMarkerTests` (marker
reading/application). Inspect their cases when selecting regression coverage.
They do not execute the game's engine or validate live signatures.

Before deployment within the user's authorized scope: confirm the game is
closed, preserve a backup of the installed ASI, copy the exact verified artifact,
and compare SHA-256. Relaunch and confirm the actual loaded module/build, then
run the feature ledger. Keep build, installed and release-package hashes aligned.
Do not overwrite an in-use ASI. Do not include personal settings, logs, profiles,
backups or saves in a public package.

Inspect both staged and unstaged diffs before a scoped commit. Include only the
repair's files; verify that the intended substantive logic is actually present.
State separately what passed static checks, build/CTest and live testing.

## 7. Record the next verified repair

Append a compact entry here after an authorized maintenance task:

```text
Date / source commit / build variant:
Game path / TU / full PE version / EXE SHA-256 / live module base:
Built ASI path and hash / installed ASI path and hash:
Observed failing action and reproduction:
Root cause:
Function / signature symbol / old and new RVA / match count:
Evidence for ABI, member offsets, global indirection and realm:
Files changed and regression checks:
Build and CTest result:
Live actions passed / failed / not tested:
Remaining work and next precise diagnostic step:
```

Older background: [REVERSE_ENGINEERING_GUIDE.md](REVERSE_ENGINEERING_GUIDE.md),
[README_TU200_OFFSETS.md](README_TU200_OFFSETS.md),
[TU200_RE_NOTES.md](TU200_RE_NOTES.md). Treat their addresses, TLS constants,
support claims and version assumptions as historical until corroborated by the
current source and exact running game. Prefer symbol searches to stale line numbers.
