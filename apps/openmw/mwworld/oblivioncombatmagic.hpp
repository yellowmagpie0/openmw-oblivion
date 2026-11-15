#ifndef OPENMW_MWWORLD_OBLIVIONCOMBATMAGIC_H
#define OPENMW_MWWORLD_OBLIVIONCOMBATMAGIC_H

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <components/esm/formkey.hpp>
#include <components/esm4/potionattributes.hpp>
#include <components/esm4/ingredientuse.hpp>
namespace ESM4 { struct Potion; struct Ingredient; struct RuntimeState; struct Book; struct SpellCastState; struct Spell; struct Weapon; struct RuntimeReferenceState; }
namespace MWBase { class World; }

namespace MWWorld
{
    class ESMStore;
    class Ptr;
    class ConstPtr;
    class World;

    // Native INGR data/presentation and first-effect Player ingestion only.
    std::optional<ESM4::IngredientFatigueProfile> resolveOblivionIngredientFatigue(
        const ESMStore& store, const ESM4::Ingredient& ingredient);
    std::optional<ESM4::IngredientFatigueProfile> resolveOblivionIngredientHealthRestoration(
        const ESMStore& store, const ESM4::Ingredient& ingredient);
    bool consumeOblivionIngredient(MWBase::World& world, const Ptr& item, const Ptr& actor);
    std::string describeOblivionIngredient(MWBase::World& world, const ConstPtr& item);
    void validateOblivionIngredientRestoration(const ESMStore& store, const ESM4::RuntimeState& state);

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
    bool supportsOblivionStarOfWest(const ESMStore& store, const ESM4::Spell& spell);

    // Complete admission for instant self restoration; no partial application.
    std::optional<std::array<float, 3>> resolveOblivionPotionRestoration(
        const ESMStore& store, const ESM4::Potion& potion);

    // Original Mead profile: two recoverable attribute drains and instant REFA2.
    // Resolution does not apply effects or spend inventory.
    std::optional<ESM4::PotionAttributeState> resolveOblivionPotionAttributes(
        const ESMStore& store, const ESM4::Potion& potion);
    bool consumeOblivionPotionAttributes(MWBase::World& world, const Ptr& item, const Ptr& actor);
    std::string describeOblivionPotion(MWBase::World& world, const ConstPtr& item);
    void validateOblivionPotionAttributes(const ESMStore& store, const ESM4::RuntimeState& state);
    bool consumeOblivionPotionLight(World& world, const Ptr& item, const Ptr& actor, const ESM4::Potion& potion);
    void validateOblivionPotionLight(const ESMStore& store, const ESM4::RuntimeState& state);

    std::optional<std::array<float, 3>> resolveOblivionSpellRestoration(
        const ESMStore& store, const ESM4::Spell& spell);

    // Source-reference-owned fire on Player, independent of actor casting,
    // projectiles and the original trigger script's reactivation timer.
    struct OblivionEnvironmentalFire
    {
        ESM::FormKey mSpell;
        float mMagnitude = 0;
        float mDuration = 0;
        float mElapsed = 0;
        friend bool operator==(const OblivionEnvironmentalFire&, const OblivionEnvironmentalFire&) = default;
    };
    std::optional<OblivionEnvironmentalFire> resolveOblivionEnvironmentalFire(
        const ESMStore& store, const ESM4::Spell& spell);
    // The existing saved reference custom-state namespace retains the owned
    // effect. Absent fields mean no effect, including in older saves. Partial
    // or malformed ownership is rejected; unrelated reference state survives.
    std::optional<OblivionEnvironmentalFire> readOblivionEnvironmentalFire(
        const ESM4::RuntimeReferenceState& reference);
    void writeOblivionEnvironmentalFire(ESM4::RuntimeReferenceState& reference,
        const std::optional<OblivionEnvironmentalFire>& effect);

    struct OblivionTargetDamage
    {
        std::uint32_t mEffectCode;
        float mMagnitude;
        std::uint8_t mResistanceActorValue;
        float mProjectileSpeedFactor;
        std::string mModel;
    };

    // Original Player-owned Voice of the Emperor only. Reference custom state
    // owns the recipient, caster and simulation timer independently of AVs.
    struct OblivionCharm
    {
        ESM::FormKey mSpell;
        ESM::FormKey mCaster;
        float mMagnitude = 30;
        float mElapsed = 0;
        friend bool operator==(const OblivionCharm&, const OblivionCharm&) = default;
    };
    std::optional<OblivionTargetDamage> resolveOblivionVoiceOfEmperor(
        const ESMStore& store, const ESM4::Spell& spell);
    std::optional<OblivionCharm> readOblivionCharm(const ESM4::RuntimeReferenceState& reference);
    void writeOblivionCharm(ESM4::RuntimeReferenceState& reference, const std::optional<OblivionCharm>& effect);
    std::int32_t oblivionCharmModifier(const ESM4::RuntimeState& state,
        const ESM::FormKey& actor, const ESM::FormKey& viewer);

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

    // Audited single-use BOOK/ENCH target-fire source. Item selection is
    // independent of learned spells; the admitted cast owns its spent source.
    std::optional<OblivionTargetDamage> resolveOblivionScrollDamage(const ESMStore& store, const ESM4::Book& book);
    std::optional<ESM::FormKey> resolveOblivionScrollChameleon(const ESMStore& store, const ESM4::Book& book);
    bool releaseOblivionScrollChameleon(MWBase::World& world, const ESM::FormKey& scroll);
    void validateOblivionScrollChameleon(const ESMStore& store, const ESM4::RuntimeState& state);
    std::string describeOblivionScroll(MWBase::World& world, const ConstPtr& item);
    bool selectOblivionScroll(MWBase::World& world, const Ptr& item, const Ptr& actor);
    bool beginOblivionScrollCast(MWBase::World& world, const Ptr& item, ESM4::SpellCastState cast);

    // Winning TES4 store preflight for the narrow default self-PARA owner.
    // No forms, actor state, selected spells or RNG are changed. The caller
    // owns Oblivion profile dispatch and post-contact actor/caster eligibility.
    bool supportsOblivionDefaultCombatParalysis(const ESMStore& store);
}

#endif
