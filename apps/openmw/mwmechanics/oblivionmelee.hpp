#ifndef OPENMW_MWMECHANICS_OBLIVIONMELEE_H
#define OPENMW_MWMECHANICS_OBLIVIONMELEE_H

#include <cstdint>
#include <optional>
#include <components/esm4/physicalcombat.hpp>
#include "../mwworld/ptr.hpp"
#include "oblivioncombat.hpp"

namespace ESM4 { struct GameSetting; }
namespace MWBase { class World; }
namespace MWPhysics { class Actor; }
namespace MWRender { class Animation; }

namespace MWMechanics
{
    // Shared raw KF timing/Blend inputs for contact preparation and playback.
    OblivionMeleeSequenceMetadata captureOblivionMeleeSequenceMetadata(
        MWRender::Animation& animation, std::string_view group, bool reaction);

    struct OblivionMeleeAcquisitionSettings
    {
        float mConeDegrees;
        float mSlopeDifference;
    };
    // Winning native records override verified original compiled defaults.
    OblivionMeleeAcquisitionSettings resolveOblivionMeleeAcquisitionSettings(
        std::span<const ESM4::GameSetting* const> settings);

    // Physical body adapter: positions/vertical bounds come from the current
    // collision body; radial scale is the separately resolved native actor scale.
    ESM4::MeleeDistanceActor oblivionMeleeBody(const MWPhysics::Actor& body,
        float combatScale, bool swimming);

    // Read-only query for a pending, owned native action. nullopt means that
    // the action or physical context is unavailable; an engaged empty Ptr is
    // a physical miss. The caller resolves weapon/creature reach. No ID or AV
    // changes, damage, animation or contact publication occurs here.
    std::optional<MWWorld::Ptr> acquireOblivionMeleeContact(MWBase::World& world,
        std::uint64_t actionId, const MWWorld::Ptr& attacker,
        const MWWorld::Ptr& selectedTarget, float reach);
    // Readiness uses the same selected-target collision/LOS query without
    // allocating an action or publishing a contact.
    bool canReachOblivionMeleeTarget(MWBase::World& world, const MWWorld::Ptr& attacker,
        const MWWorld::Ptr& target, float reach);
    // S4's explicit deterministic stationary CSTY profile. Returns true while
    // combat owns package execution. Full movement/style policy belongs to S7.
    bool updateOblivionStationaryMeleeAi(MWBase::World& world, const MWWorld::Ptr& actor, bool enabled);

    struct OblivionOrdinaryContactResult
    {
        MWWorld::Ptr mVictim; // Empty means an acquired physical miss.
        ESM4::PhysicalContactDamage mDamage{0, 0};
        float mBlockFatigueDebit = 0;
        float mBlockAbsorbedFraction = 0;
        std::optional<float> mWeaponConditionAfterWear{};
        unsigned mArmorConditionWrites = 0;
        unsigned mRandomDraws = 0;
        std::optional<std::array<std::string_view, 3>> mHitSounds{};
        std::optional<std::array<float, 3>> mHitSoundPosition{};
    };
    // Acquire actual collision/LOS contact, prepare native damage, then publish
    // one owned transaction. Unavailable and unsupported contexts do not spend.
    std::optional<OblivionOrdinaryContactResult> commitOblivionOrdinaryMeleeContact(
        MWBase::World& world, std::uint64_t actionId, const MWWorld::Ptr& attacker,
        const MWWorld::Ptr& selectedTarget, float reach, float weaponWeight,
        float normalizedDifficulty, bool sneaking);
    std::optional<OblivionOrdinaryContactResult> commitOblivionMeleeContact(
        MWBase::World& world, std::uint64_t actionId, const MWWorld::Ptr& attacker,
        const MWWorld::Ptr& selectedTarget, float reach, float weaponWeight,
        float normalizedDifficulty, bool sneaking);

    // Read-only Counter phase1 consequence after geometric acquisition. The
    // packet includes the separate Master disarm roll followed by Stagger
    // resource selection. Unsupported drops/resources reject without spending.
    std::optional<OblivionPhysicalContactDeltas> prepareOblivionCounterContactDeltas(
        MWBase::World& world, std::uint64_t actionId, const MWWorld::Ptr& attacker,
        const MWWorld::Ptr& victim);

    // Adapter for this fork's existing -100..100 GUI slider. Config values
    // outside that UI range saturate; this is not native INI deserialization.
    float oblivionNormalizedDifficulty(int difficultySetting);

    // Read native equipped condition and AV authority. This is a query, not
    // damage, block, wear or effect execution; shared TES3 ratings are not used.
    // Resolve actual native actor binding before reading its process byte.
    std::int16_t oblivionProcessAction(MWBase::World& world, const MWWorld::Ptr& actor);
    bool oblivionBlockingPosture(MWBase::World& world, const MWWorld::Ptr& actor);
    bool oblivionParalyzed(MWBase::World& world, const MWWorld::Ptr& actor);

    std::int8_t oblivionKnockedState(MWBase::World& world, const MWWorld::Ptr& actor);

    // Pre-mitigation unarmed contact damage from live native integer AV getters,
    // current/base Fatigue and the actual victim process byte. Read-only;
    // creature natural attacks use a separate native damage path.
    ESM4::HandToHandDamage oblivionHandToHandContactDamage(MWBase::World& world,
        const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim);

    // Creature +34C reads the current form's scaled damage word, applies its
    // Fatigue factor, truncates to integer, then returns floating Health damage.
    // It writes no hand-to-hand Fatigue damage and has no NPC power multiplier.
    ESM4::HandToHandDamage oblivionCreatureNaturalContactDamage(
        MWBase::World& world, const MWWorld::Ptr& attacker);

    // Read-only ordinary unarmed hit policy. Unsupported contact branches
    // return nullopt; malformed or missing authority is diagnosed. Geometry
    // and owned-action admission remain the contact caller's responsibility.
    struct OblivionUnarmedContactDamage
    {
        float mHealth;
        float mFatigue;
        float mBlockFatigueDebit = 0;
        float mBlockAbsorbedFraction = 0;
        float mPreArmorDamage = 0;
        std::vector<OblivionPhysicalConditionChange> mVictimConditionChanges{};
        std::optional<OblivionCombatRandomTransition> mRandomTransition{};
        bool mBlockAccepted = false;
        bool mWeaponBlockSound = false;
        std::optional<std::array<std::string_view, 3>> mBlockSounds{};
        std::int32_t mHitArmorClass = -1; // Native contact-selected material, independent of condition writes.
    };
    std::optional<OblivionUnarmedContactDamage> resolveOblivionOrdinaryUnarmedContact(
        MWBase::World& world, const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim,
        float normalizedDifficulty, bool sneaking);
    std::optional<OblivionUnarmedContactDamage> resolveOblivionUnarmedMeleeContact(
        MWBase::World& world, const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim,
        float normalizedDifficulty, bool sneaking, ESM4::MeleeStrikeKind kind);

    // Read-only pre-mitigation ordinary melee WEAP query on the actual equipped
    // instance. Native float AV conversions and integer AttackBonus are distinct.
    struct OblivionWeaponContactDamage
    {
        float mHealth;
        std::optional<float> mConditionAfterWear;
        std::vector<OblivionPhysicalConditionChange> mVictimConditionChanges{};
        std::optional<OblivionCombatRandomTransition> mRandomTransition{};
        float mPreArmorDamage = 0;
        float mBlockFatigueDebit = 0;
        float mBlockAbsorbedFraction = 0;
        bool mBlockAccepted = false;
        bool mWeaponBlockSound = false;
        std::optional<std::array<std::string_view, 3>> mBlockSounds{};
        std::int32_t mHitArmorClass = -1;
        OblivionWeaponBreakDisposition mBreakDisposition = OblivionWeaponBreakDisposition::None;
    };
    // Read-only ordinary weapon contact preparation. Armor selection, equipped
    // blocking, broken-item reactions and enchantment hooks gate admission.
    std::optional<OblivionWeaponContactDamage> resolveOblivionOrdinaryWeaponContact(
        MWBase::World& world, const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim,
        const MWWorld::Ptr& item, float normalizedDifficulty, bool sneaking);
    std::optional<OblivionWeaponContactDamage> resolveOblivionWeaponMeleeContact(
        MWBase::World& world, const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim,
        const MWWorld::Ptr& item, float normalizedDifficulty, bool sneaking, ESM4::MeleeStrikeKind kind);

    float oblivionOrdinaryWeaponContactDamage(MWBase::World& world,
        const MWWorld::Ptr& attacker, const MWWorld::Ptr& item);

    float oblivionArmorRating(MWBase::World& world, const MWWorld::Ptr& actor);

}
#endif
