#ifndef OPENMW_MWMECHANICS_OBLIVIONCOMBAT_H
#define OPENMW_MWMECHANICS_OBLIVIONCOMBAT_H

#include <components/esm4/runtimestate.hpp>
#include <components/esm4/actorvalues.hpp>
#include <components/esm4/physicalcombat.hpp>
#include <components/esm/refid.hpp>

#include "stat.hpp"

#include <array>
#include <map>
#include <span>

namespace MWWorld
{
    class Ptr;
    class Player;
    class ESMStore;
}

namespace MWMechanics
{
    class CreatureStats;
    class NpcStats;

    struct OblivionActorProjectionInput
    {
        ESM4::ActorValueOwner mOwner = ESM4::ActorValueOwner::NonPlayer;
        ESM4::ActorValueProcess mProcess = ESM4::ActorValueProcess::Active;
        std::array<ESM4::ActorValueState, 8> mAttributes{};
        std::array<ESM4::ActorValueState, 21> mSkills{};
        // Prepared native base, maximum and current, in health/magicka/fatigue
        // order. Their AV-specific formulas belong to the native authority.
        std::array<std::array<float, 3>, 3> mDynamic{};
    };

    // A synchronous transaction: the target must outlive this object. Prepare
    // all projections before committing authority and views, without callbacks
    // between the commits. This object neither owns nor serializes native AVs.
    class OblivionActorProjection
    {
        CreatureStats& mTarget;
        std::array<AttributeValue, 8> mAttributes;
        std::array<AttributeValue*, 8> mAttributeTargets{};
        std::array<AttributeValue, 21> mSkills;
        std::array<AttributeValue*, 21> mSkillTargets{};
        std::array<DynamicStat<float>, 3> mDynamic;
        bool mCommitted = false;

        static void replaceAttribute(AttributeValue& target, const AttributeValue& value) noexcept;
        OblivionActorProjection(CreatureStats& target, NpcStats* npc,
            const OblivionActorProjectionInput& input);

    public:
        OblivionActorProjection(CreatureStats& target, const OblivionActorProjectionInput& input);
        OblivionActorProjection(NpcStats& target, const OblivionActorProjectionInput& input);
        OblivionActorProjection(const OblivionActorProjection&) = delete;
        OblivionActorProjection& operator=(const OblivionActorProjection&) = delete;
        // False on a repeated call: never swap the old views back into place.
        bool commit() noexcept;
    };

    struct OblivionFatigueSettings
    {
        ESM4::MovementFatigueSettings mMovement;
        ESM4::FatigueRegenerationSettings mRegeneration;
        ESM4::CombatMasterySettings mMastery;
        ESM4::PlayerDynamicBaseSettings mPlayerBase;
    };

    struct OblivionFatigueUpdate
    {
        float mDuration;
        std::int32_t mEncumbrance;
        bool mRunning;
        bool mCanSpend;
    };

    struct OblivionRestorationSettings
    {
        ESM4::MagickaRegenerationSettings mMagicka;
        ESM4::FatigueRegenerationSettings mFatigue;
        ESM4::PlayerDynamicBaseSettings mPlayerBase;
    };

    struct OblivionRestorationUpdate
    {
        float mDuration; // Elapsed seconds, after caller-owned effect advancement.
        bool mRestoreHealth;
        bool mHasActiveMagicItem;
    };

    // Profile-owned native action and actor-value authority. Live activation
    // and contact transitions are wired separately; issuing an ID is not a hit.
    class OblivionCombatService
    {
        class PreparedNonPlayerView;
        ESM4::ActionLedger mActions;
        std::map<ESM::FormKey, ESM4::RuntimeActorValues> mActorValues;
        std::map<ESM::FormKey, ESM4::RuntimeActorBaseOverride> mActorBases;
        const ESM4::RuntimeActorBaseOverride* findActorBase(const ESM::FormKey& base) const;
        const ESM4::RuntimeActorValues& playerValues() const;
        const ESM4::RuntimeActorValues& nonPlayerValues(const MWWorld::Ptr& actor) const;
        const ESM4::RuntimeActorValues& nonPlayerValues(const ESM::FormKey& actor) const;

    public:
        void clear();
        std::uint64_t allocateAction();
        bool isActionPending(std::uint64_t id) const;
        bool isActionConsumed(std::uint64_t id) const;
        // Internal completion/cancellation boundary. Call only with the
        // corresponding gameplay transition committed, before event callbacks.
        bool consumeAction(std::uint64_t id);
        // Explicit construction/load publication. TES4 NPCs and creatures
        // are supported here; automatic gameplay activation is wired separately.
        void publishNonPlayerValues(const MWWorld::Ptr& actor, ESM4::RuntimeActorValues values);
        // Caller applies eligibility, event and death policy before/after this
        // scalar transition. This method cannot run callbacks between commits.
        void changeNonPlayerValue(const MWWorld::Ptr& actor, std::uint8_t value,
            ESM4::ActorValueModifier modifier, float delta);
        // Shared base-record transaction. The caller supplies every resident,
        // published reference of this base, including actor. Unloaded saved
        // actors change too; newly published references inherit the override.
        // No pointers survive the call. Event/cache policy remains caller-owned.
        void setNonPlayerBaseValue(const MWWorld::Ptr& actor, std::uint8_t value,
            std::int32_t requested, std::span<const MWWorld::Ptr> residents);
        void setPlayerBaseValue(MWWorld::Player& player, std::uint8_t value,
            std::int32_t requested, const ESM4::PlayerDynamicBaseSettings& settings);
        float getNonPlayerValue(const MWWorld::Ptr& actor, std::uint8_t value) const;
        std::int32_t getNonPlayerIntegerValue(const MWWorld::Ptr& actor, std::uint8_t value) const;
        // Read saved actors without constructing/loading their live references.
        // Content references must still name a matching winning ACHR/ACRE.
        float getNonPlayerValue(const ESM::FormKey& actor, std::uint8_t value, const MWWorld::ESMStore& store) const;
        std::int32_t getNonPlayerIntegerValue(
            const ESM::FormKey& actor, std::uint8_t value, const MWWorld::ESMStore& store) const;
        // Native GetBaseAV returns the integer base query. Creature base skill
        // groups differ from runtime aliases; Encumbrance here is capacity.
        std::int32_t getNonPlayerBaseValue(
            const ESM::FormKey& actor, std::uint8_t value, const MWWorld::ESMStore& store) const;
        std::int32_t getPlayerBaseValue(std::uint8_t value) const;
        // Raw player form contributions must be present. Settings are winning
        // runtime inputs, not save-owned values. Recompute derived bases before
        // preparing all shared views; never feed resolved bases back as inputs.
        void publishPlayerValues(MWWorld::Player& player, ESM4::RuntimeActorValues values,
            const ESM4::PlayerDynamicBaseSettings& settings);
        void changePlayerValue(MWWorld::Player& player, std::uint8_t value,
            ESM4::ActorValueModifier modifier, float delta, const ESM4::PlayerDynamicBaseSettings& settings);
        // Regeneration restores the native Damage channel. It must not clamp
        // the result through a TES3 DynamicStat or discard sparse presence.
        void regenerateNonPlayerFatigue(const MWWorld::Ptr& actor, float duration,
            const ESM4::FatigueRegenerationSettings& settings);
        void regeneratePlayerFatigue(MWWorld::Player& player, float duration,
            const ESM4::FatigueRegenerationSettings& settings, const ESM4::PlayerDynamicBaseSettings& baseSettings);
        // Prepare Health, Magicka, then Fatigue and publish once. Callers own
        // rest/wait/jail eligibility and active-item/effect lifecycle.
        void restoreNonPlayerResources(const MWWorld::Ptr& actor, const OblivionRestorationUpdate& input,
            const OblivionRestorationSettings& settings);
        void restorePlayerResources(MWWorld::Player& player, const OblivionRestorationUpdate& input,
            const OblivionRestorationSettings& settings);
        // Running expenditure precedes regeneration; prepare and publish both
        // as one transition so failure cannot leave a partially updated actor.
        void updateNonPlayerFatigue(const MWWorld::Ptr& actor, const OblivionFatigueUpdate& input,
            const OblivionFatigueSettings& settings);
        void updatePlayerFatigue(MWWorld::Player& player, const OblivionFatigueUpdate& input,
            const OblivionFatigueSettings& settings);
        void spendPlayerJumpFatigue(MWWorld::Player& player, std::int32_t encumbrance, bool canSpend,
            const OblivionFatigueSettings& settings);
        float getPlayerValue(std::uint8_t value) const;
        std::int32_t getPlayerIntegerValue(std::uint8_t value) const;
        const ESM4::RuntimeActorValues* findActorValues(const ESM::FormKey& actor) const;
        void capture(ESM4::RuntimeState& state) const;
        void restore(const ESM4::RuntimeState& state);
        // World load preflight: validate winning native actor bindings before
        // replacing either action ownership or actor values.
        void restore(const ESM4::RuntimeState& state, const MWWorld::ESMStore& store);
    };
}

#endif
