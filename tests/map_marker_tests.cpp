#include "game/map_marker.h"

#include <cstdio>
#include <limits>

namespace
{
    constexpr uintptr_t global = 0x100000, ui = 0x200000, destination = 0x300000;
    uintptr_t currentUi = ui, currentDestination = destination;
    trinity::game::MapMarkerPosition current{-10691.016602f, 0.0f, -3722.970459f};
    bool readable = true, replaceDuringRead = false;
    int failures = 0;

    bool ReadPointer(uintptr_t address, uintptr_t* out)
    {
        if (address == global) { *out = currentUi; return true; }
        if (address == ui + 0xA8) { *out = currentDestination; return true; }
        return false;
    }

    bool ReadPosition(uintptr_t address, float* out)
    {
        if (!readable || address != destination + 0x20) return false;
        out[0] = current.x; out[1] = current.y; out[2] = current.z;
        if (replaceDuringRead) currentDestination = 0x400000;
        return true;
    }

    void Expect(bool condition, const char* message)
    {
        if (!condition) { std::printf("FAIL: %s\n", message); ++failures; }
    }
}

int main()
{
    using namespace trinity::game;
    MapMarkerPosition out{};
    const auto read = [&] { return ReadCurrentMapMarker(global, ReadPointer, ReadPosition, out); };
    Expect(read() && out.x == current.x && out.z == current.z,
           "existing TU 2.01.00 map destination is readable without a capture-hook event");
    current = {-10940.327148f, 0.0f, -3814.648682f};
    Expect(read() && out.x == current.x && out.z == current.z,
           "moving the marker replaces the previous coordinates; zero altitude is valid");
    current = {};
    Expect(!read(), "removing the map destination must not return a cached point");
    current = {1.0f, std::numeric_limits<float>::quiet_NaN(), 2.0f};
    Expect(!read(), "non-finite destination is rejected");
    current = {1.1e9f, 0.0f, 2.0f};
    Expect(!read(), "out-of-range destination is rejected");
    current = {1.0f, 0.0f, 2.0f};
    readable = false;
    Expect(!read(), "unreadable destination is rejected");
    readable = true;
    currentUi = 0;
    Expect(!read(), "missing UI during loading is rejected");
    currentUi = ui;
    currentDestination = 0;
    Expect(!read(), "missing destination state is rejected");
    currentDestination = destination;
    replaceDuringRead = true;
    Expect(read(), "a valid destination remains usable when the game replaces its object after reading");
    if (!failures) std::puts("map marker tests passed");
    return failures ? 1 : 0;
}
