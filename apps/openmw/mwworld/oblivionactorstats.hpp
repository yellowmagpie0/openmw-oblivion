#ifndef OPENMW_MWWORLD_OBLIVIONACTORSTATS_H
#define OPENMW_MWWORLD_OBLIVIONACTORSTATS_H

#include <components/esm/formkey.hpp>
#include <components/esm/refid.hpp>
#include <components/esm4/actorvalues.hpp>
#include <components/esm4/physicalcombat.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace MWMechanics
{
    struct OblivionFatigueSettings;
}

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

    const std::array<ESM::RefId, 21>& oblivionSkillIds();
    // Resolve native winning GMSTs at the publication boundary. Shared TES3
    // aliases and previously cached settings cannot supply player formulas.
    ESM4::PlayerDynamicBaseSettings resolveOblivionPlayerDynamicBaseSettings(const ESMStore& store);
    ESM4::FatigueRegenerationSettings resolveOblivionFatigueRegenerationSettings(const ESMStore& store);
    MWMechanics::OblivionFatigueSettings resolveOblivionFatigueSettings(const ESMStore& store);
    ESM4::SwimBreathSettings resolveOblivionSwimBreathSettings(const ESMStore& store);
    ESM4::EssentialRecoverySettings resolveOblivionEssentialRecoverySettings(const ESMStore& store);
    // Uses the live player's base level only for PCLevelOffset actors.
    OblivionActorBaseStats resolveOblivionActorConstructionStats(const ESMStore& store,
        ESM::FormId actorBase, bool scaled);

    // Read-only construction inputs from native winning records. Runtime
    // modifiers/current values and persisted changes belong to actor authority.
    OblivionActorBaseStats resolveOblivionActorBaseStats(const ESMStore& store,
        const ESM::FormKey& actorBase, std::optional<std::uint16_t> playerLevel);
}

#endif
