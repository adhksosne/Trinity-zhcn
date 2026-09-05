#include "player_logic.h"

namespace trinity::game
{
    bool ShouldBlockPlayerDamage(bool godMode, bool strictPlayerTarget,
                                 bool mountTarget)
    {
        return godMode && (strictPlayerTarget || mountTarget);
    }

    bool IsMountTypeTag(uint8_t tag)
    {
        return tag == 5 || tag == 6;
    }
}
