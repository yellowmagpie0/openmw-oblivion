#ifndef OPENMW_MWWORLD_OBLIVIONACTORSTATS_H
#define OPENMW_MWWORLD_OBLIVIONACTORSTATS_H

#include <components/esm/formkey.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace MWWorld
{
    class ESMStore;

    struct OblivionActorBaseStats
    {
        std::int16_t mLevel;
        std::array<std::uint8_t, 8> mAttributes;
        std::array<std::uint8_t, 21> mSkills;
        std::uint32_t mHealth;
        std::uint16_t mMagicka;
        std::uint16_t mFatigue;
        std::optional<std::uint16_t> mNaturalDamage;
    };

    // Read-only construction inputs from native winning records. Runtime
    // modifiers/current values and persisted changes belong to actor authority.
    OblivionActorBaseStats resolveOblivionActorBaseStats(const ESMStore& store,
        const ESM::FormKey& actorBase, std::optional<std::uint16_t> playerLevel);
}

#endif
