#ifndef OPENMW_MWWORLD_OBLIVIONACTORSTATS_H
#define OPENMW_MWWORLD_OBLIVIONACTORSTATS_H

#include <components/esm/formkey.hpp>
#include <components/esm/refid.hpp>
#include <components/esm4/actorvalues.hpp>
#include <components/esm4/actorstats.hpp>
#include <components/esm4/ability.hpp>
#include <components/esm4/physicalcombat.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace MWMechanics
{
    struct OblivionFatigueSettings;
    struct OblivionFrameSettings;
}

namespace ESM
{
    struct Class;
}

namespace ESM4
{
    struct RuntimeActorValues;
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
    MWMechanics::OblivionFrameSettings resolveOblivionFrameSettings(const ESMStore& store);
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

    // Fresh native values only. Callers must prefer restored authority and
    // select the actual process before publishing these construction inputs.
    // This resolves winning forms, not abilities, effects or lifecycle events.
    ESM4::RuntimeActorValues resolveOblivionInitialNonPlayerValues(const ESMStore& store,
        const ESM::FormKey& actor, const ESM::FormKey& actorBase,
        std::optional<std::uint16_t> playerLevel, ESM4::ActorValueProcess process);

    // Native Player form calculation from the selected winning race/class,
    // original ordered signed bonuses, SKILs and GMSTs. No facade or ability
    // calculation. Full FormId class comparison follows the original branch.
    ESM4::ActorCharacterBaseStats resolveOblivionPlayerCharacterBaseStats(const ESMStore& store,
        const ESM::RefId& race, const ESM::RefId& characterClass, bool female, std::int16_t level);
    // Calculate an uncommitted custom class owned by a prepared Player record.
    // Its identity must match the proposed class; this does not publish it.
    ESM4::ActorCharacterBaseStats resolveOblivionPlayerCharacterBaseStats(const ESMStore& store,
        const ESM::RefId& race, const ESM::RefId& characterClass, bool female, std::int16_t level,
        const ESM::Class* preparedCustomClass);

    // Narrow Ability4 self effects. Resolve stable winning definitions and
    // prepared override history; reject unsupported or ambiguous inputs.
    std::vector<ESM4::PassiveAbilityInput> resolveOblivionPassiveAbilityInputs(
        const ESMStore& store, std::span<const ESM::FormKey> spells);

    struct OblivionPlayerSpellInputs
    {
        // All declared source spells, including powers and ordinary spells.
        // Declaration is not execution of their effects or casting behavior.
        std::vector<ESM::FormKey> mDeclaredSpells;
        std::vector<ESM4::PassiveAbilityInput> mPassiveAbilities;
    };

    // Winning Player form7, selected RACE and optional BSGN declarations.
    // Preserve first source occurrence and stable spell identity. This source
    // collection order does not establish original caster/active-list order.
    OblivionPlayerSpellInputs resolveOblivionPlayerSpellInputs(
        const ESMStore& store, const ESM::RefId& race, const ESM::RefId& birthSign);

    // Authored Player form inputs before character generation or abilities.
    // Derived resource bases are resolved by preparePlayerValues at publication;
    // existing save authority must take precedence over these fresh inputs.
    ESM4::RuntimeActorValues resolveOblivionInitialPlayerValues(const ESMStore& store);
}

#endif
