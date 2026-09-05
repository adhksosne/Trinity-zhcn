#pragma once

namespace trinity::game
{
    // A party-indexed actor is a stronger identity signal than a stale or
    // incomplete gear-name/TypeID classification.
    bool AcceptCharacterComponent(int selectedIndex, int identifiedIndex,
                                  int partyIndex);
}
