#pragma once

namespace trinity::game
{
    // God Mode may only block damage after the damage target has been
    // positively identified as a tracked player target (or mount).
    bool ShouldBlockPlayerDamage(bool godMode, bool strictPlayerTarget,
                                 bool mountTarget);
}
