#include "equipment_logic.h"

namespace trinity::game
{
    bool AcceptCharacterComponent(int selectedIndex, int identifiedIndex,
                                  int partyIndex)
    {
        if (selectedIndex < 0 || selectedIndex > 2) return false;
        if (partyIndex != selectedIndex) return false;
        return identifiedIndex < 0 || identifiedIndex == selectedIndex ||
               partyIndex == selectedIndex;
    }
}
