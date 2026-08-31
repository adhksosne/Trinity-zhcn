#include "xinput_hook.h"

#include <MinHook.h>
#include <Windows.h>

#include "../core/state.h"

#pragma comment(lib, "xinput9_1_0.lib")

namespace trinity::hooks
{
    using XInputGetState_t = DWORD (WINAPI*)(DWORD, XINPUT_STATE*);

    // One trampoline per known module. The game and our own overlay may link
    // different xinput versions, and each hook must forward to its own original.
    static XInputGetState_t o_1_4   = nullptr;
    static XInputGetState_t o_1_3   = nullptr;
    static XInputGetState_t o_9_1_0 = nullptr;

    // Any real-state trampoline, used by the menu to read the pad it's blocking.
    static XInputGetState_t g_read = nullptr;

    // How long after the menu closes we keep neutralizing the menu buttons.
    // The press that closed the menu (e.g. B on the last page) would otherwise
    // reach the game as a fresh button press and roll/dodge the character.
    constexpr ULONGLONG kMenuCloseEatWindowMs = 250;

    static void Neutralize(XINPUT_STATE* s)
    {
        // Only strip the buttons the menu itself uses (d-pad, A/B/X, and the
        // LB/RB section switchers). The thumbsticks, triggers and every other
        // button stay live so the player can still move and look around with
        // the menu open.
        constexpr WORD kMenuButtons =
            XINPUT_GAMEPAD_DPAD_UP  | XINPUT_GAMEPAD_DPAD_DOWN |
            XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT |
            XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B | XINPUT_GAMEPAD_X |
            XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER;

        const State& st      = State::Get();
        const ULONGLONG now  = GetTickCount64();
        const bool eatMenuKeys = st.menuOpen ||
            (st.menuCloseAt != 0 && now >= st.menuCloseAt &&
             now - st.menuCloseAt < kMenuCloseEatWindowMs);

        if (eatMenuKeys && (s->Gamepad.wButtons & kMenuButtons))
        {
            s->Gamepad.wButtons &= ~kMenuButtons;
            ++s->dwPacketNumber; // bump so the game registers the state as changed
        }
    }

    // A distinct detour per module so each can call the matching trampoline
    // (MinHook can't tell us which target a shared detour was invoked for).
    #define TRINITY_XINPUT_DETOUR(NAME, ORIG)                              \
        static DWORD WINAPI NAME(DWORD i, XINPUT_STATE* s)                 \
        {                                                                  \
            const DWORD r = ORIG(i, s);                                    \
            if (r == ERROR_SUCCESS && s && State::Get().menuOpen)         \
                Neutralize(s);                                             \
            return r;                                                      \
        }
    TRINITY_XINPUT_DETOUR(hk_1_4,   o_1_4)
    TRINITY_XINPUT_DETOUR(hk_1_3,   o_1_3)
    TRINITY_XINPUT_DETOUR(hk_9_1_0, o_9_1_0)
    #undef TRINITY_XINPUT_DETOUR

    struct Target
    {
        const wchar_t*    dll;
        void*             detour;
        XInputGetState_t* original;
        bool              done; // hooked, or confirmed nothing to hook here
    };

    static Target g_targets[] = {
        { L"xinput1_4.dll",   reinterpret_cast<void*>(&hk_1_4),   &o_1_4,   false },
        { L"xinput1_3.dll",   reinterpret_cast<void*>(&hk_1_3),   &o_1_3,   false },
        { L"xinput9_1_0.dll", reinterpret_cast<void*>(&hk_9_1_0), &o_9_1_0, false },
    };

    void EnsureXInputHooks()
    {
        static bool s_allDone = false;
        if (s_allDone)
            return;

        bool anyPending = false;
        for (auto& t : g_targets)
        {
            if (t.done)
                continue;

            HMODULE mod = GetModuleHandleW(t.dll);
            if (!mod)
            {
                anyPending = true; // may still load (game inits input after us)
                continue;
            }

            FARPROC proc = GetProcAddress(mod, "XInputGetState");
            if (!proc)
            {
                t.done = true; // module present but no export - never retry it
                continue;
            }

            if (MH_CreateHook(reinterpret_cast<void*>(proc), t.detour,
                              reinterpret_cast<void**>(t.original)) != MH_OK)
            {
                anyPending = true; // transient; try again next frame
                continue;
            }

            // Publish the reader before enabling so a concurrent XInputReadReal
            // never falls through to a now-patched plain export.
            if (!g_read)
                g_read = *t.original;

            if (MH_EnableHook(reinterpret_cast<void*>(proc)) == MH_OK)
            {
                t.done = true;
            }
            else
            {
                anyPending = true;
            }
        }

        if (!anyPending)
            s_allDone = true;
    }

    void RemoveXInputHooks()
    {
        // The detours are torn down by MH_DisableHook(MH_ALL_HOOKS) /
        // MH_Uninitialize during shutdown; just reset our bookkeeping.
        for (auto& t : g_targets)
        {
            t.done      = false;
            *t.original = nullptr;
        }
        g_read = nullptr;
    }

    DWORD XInputReadReal(DWORD userIndex, XINPUT_STATE* state)
    {
        if (g_read)
            return g_read(userIndex, state);
        return XInputGetState(userIndex, state); // hooks not up yet
    }

    // --- Cached pad scan ------------------------------------------------------
    // See header for the rationale. Plain statics (not atomics): the two
    // callers are the render thread and the game tick; a benign race at worst
    // causes one redundant scan window, never a wrong merged state.
    static DWORD     g_liveSlots  = 0;   // bitmask of slots that answered recently
    static ULONGLONG s_nextRescan = 0;   // next full 4-slot scan deadline
    static ULONGLONG s_cacheT     = 0;   // when the merged state below was taken
    static bool      s_cacheOk    = false;
    static XINPUT_STATE s_cache   = {};

    bool ReadPadsCached(XINPUT_STATE& merged)
    {
        const ULONGLONG now = GetTickCount64();

        if (now < s_cacheT + 4 && now >= s_cacheT)
        {
            merged = s_cache;           // within the refresh window
            return s_cacheOk;
        }

        if (now >= s_nextRescan)
        {
            DWORD mask = 0;
            for (DWORD i = 0; i < 4; ++i)
            {
                XINPUT_STATE st;
                if (XInputReadReal(i, &st) == ERROR_SUCCESS)
                    mask |= (1u << i);
            }
            g_liveSlots  = mask;
            s_nextRescan = now + 2000;
        }

        ZeroMemory(&merged, sizeof(merged));
        bool any = false;
        for (DWORD i = 0; i < 4; ++i)
        {
            if (!(g_liveSlots & (1u << i)))
                continue;
            XINPUT_STATE st;
            if (XInputReadReal(i, &st) == ERROR_SUCCESS)
            {
                any = true;
                merged.Gamepad.wButtons |= st.Gamepad.wButtons;
                if (st.Gamepad.bLeftTrigger  > merged.Gamepad.bLeftTrigger)
                    merged.Gamepad.bLeftTrigger  = st.Gamepad.bLeftTrigger;
                if (st.Gamepad.bRightTrigger > merged.Gamepad.bRightTrigger)
                    merged.Gamepad.bRightTrigger = st.Gamepad.bRightTrigger;
                if (merged.Gamepad.sThumbLX == 0 && merged.Gamepad.sThumbLY == 0)
                {
                    merged.Gamepad.sThumbLX = st.Gamepad.sThumbLX;
                    merged.Gamepad.sThumbLY = st.Gamepad.sThumbLY;
                }
                if (merged.Gamepad.sThumbRX == 0 && merged.Gamepad.sThumbRY == 0)
                {
                    merged.Gamepad.sThumbRX = st.Gamepad.sThumbRX;
                    merged.Gamepad.sThumbRY = st.Gamepad.sThumbRY;
                }
            }
            else
            {
                g_liveSlots &= ~(1u << i); // dropped; the 2s rescan re-adds it
            }
        }

        s_cache   = merged;
        s_cacheOk = any;
        s_cacheT  = now;
        return any;
    }
}
