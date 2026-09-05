#include "player_logic.h"

namespace trinity::game
{
    bool ShouldBlockPlayerDamage(bool godMode, bool strictPlayerTarget,
                                 bool mountTarget)
    {
        return godMode && (strictPlayerTarget || mountTarget);
    }
}
