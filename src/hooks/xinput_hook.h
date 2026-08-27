#pragma once
#include <Windows.h>
#include <Xinput.h>

namespace trinity::hooks
{
    // Detours XInputGetState so the pad is neutralised for the GAME while the
    // menu is open - stops nav buttons (A/B, d-pad, RB+X) leaking through. The
    // menu reads the real pad via XInputReadReal, so its own navigation still
    // works while it blocks the game.
    //
    // Safe to call every frame: it hooks whichever xinput module has since
    // loaded (the game often inits its input system after we do) and becomes a
    // no-op once every known module is accounted for.
    void EnsureXInputHooks();
    void RemoveXInputHooks();

    // Real pad state, bypassing the menu-open neutralisation applied to the
    // game. Falls back to the plain export until the hooks are up.
    DWORD XInputReadReal(DWORD userIndex, XINPUT_STATE* state);

    // Cached merged pad state across all live slots.
    //
    // Perf: XInputGetState on a connected pad can block ~1ms (USB poll sync).
    // The old pattern - every caller scanning all 4 slots, several callers per
    // frame - cost 2-4ms per frame at high refresh rates and halved FPS with a
    // controller attached (CPU-bound, GPU idling). This polls at most one
    // refresh per ~4ms window, only slots that recently answered, and rescans
    // all four slots every 2s so hot-plugs are still picked up.
    //
    // Merges button/trigger states and first-moved thumbsticks across slots.
    // Returns true if any pad answered. Safe from multiple threads.
    bool ReadPadsCached(XINPUT_STATE& merged);
}
