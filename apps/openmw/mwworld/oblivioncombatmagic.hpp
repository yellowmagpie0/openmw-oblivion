#ifndef OPENMW_MWWORLD_OBLIVIONCOMBATMAGIC_H
#define OPENMW_MWWORLD_OBLIVIONCOMBATMAGIC_H

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <components/esm/formkey.hpp>
namespace ESM4 { struct Potion; struct Spell; struct Weapon; }
namespace MWBase { class World; }

namespace MWWorld
{
    class ESMStore;

    struct OblivionSpellPresentation
    {
        std::string mDescription;
        std::string mIcon;
        std::optional<float> mCost;
    };
    // Native display and cost inputs only; no selection, resource write or cast.
    OblivionSpellPresentation describeOblivionSpell(const ESMStore& store, const ESM4::Spell& spell,
        const std::array<std::int32_t, 6>& schoolSkills, std::int32_t luck);
    OblivionSpellPresentation describeOblivionPlayerSpell(MWBase::World& world, const ESM4::Spell& spell);

    // Complete admission for instant self restoration; no partial application.
    std::optional<std::array<float, 3>> resolveOblivionPotionRestoration(
        const ESMStore& store, const ESM4::Potion& potion);

    std::optional<std::array<float, 3>> resolveOblivionSpellRestoration(
        const ESMStore& store, const ESM4::Spell& spell);

    struct OblivionTargetDamage
    {
        std::uint32_t mEffectCode;
        float mMagnitude;
        std::uint8_t mResistanceActorValue;
        float mProjectileSpeedFactor;
        std::string mModel;
    };

    struct OblivionStaffCastInputs
    {
        ESM::FormKey mEnchantment;
        float mAuthoredChargeCost;
        float mMaximumCharge;
        OblivionTargetDamage mDamage;
    };

    // Admit complete instant, single-target fire/shock effects. These queries
    // resolve winning content only; they do not equip, spend charge or launch.
    std::optional<OblivionTargetDamage> resolveOblivionTargetSpellDamage(
        const ESMStore& store, const ESM4::Spell& spell);
    std::optional<OblivionStaffCastInputs> resolveOblivionStaffCastInputs(
        const ESMStore& store, const ESM4::Weapon& weapon);

    // Winning TES4 store preflight for the narrow default self-PARA owner.
    // No forms, actor state, selected spells or RNG are changed. The caller
    // owns Oblivion profile dispatch and post-contact actor/caster eligibility.
    bool supportsOblivionDefaultCombatParalysis(const ESMStore& store);
}

#endif
