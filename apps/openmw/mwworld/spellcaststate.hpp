#ifndef GAME_MWWORLD_SPELLCASTSTATE_H
#define GAME_MWWORLD_SPELLCASTSTATE_H

namespace MWWorld
{
    enum class SpellCastState
    {
        Success = 0,
        InsufficientMagicka = 1,
        PowerAlreadyUsed = 2,
        Unsupported = 3
    };
}

#endif
