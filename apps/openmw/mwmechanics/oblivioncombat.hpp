#ifndef OPENMW_MWMECHANICS_OBLIVIONCOMBAT_H
#define OPENMW_MWMECHANICS_OBLIVIONCOMBAT_H

#include <components/esm4/runtimestate.hpp>
#include <components/esm4/actorvalues.hpp>
#include <components/esm4/physicalcombat.hpp>
#include <components/esm/refid.hpp>

#include "stat.hpp"

#include <array>
#include <map>
#include <deque>
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
        // Optional during staged native activation. Absence preserves the
        // target's existing lifecycle view; presence is authoritative.
        std::optional<ESM4::ActorLifePhase> mLife;
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
        std::optional<ESM4::ActorLifePhase> mLife;
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

    struct OblivionActorValueCommandResult
    {
        // False means eligibility suppressed both storage and notifications.
        bool mAccepted;
        // Caller dispatches the negative-Health callback after this transaction.
        std::optional<float> mHealthReactionDelta;
    };

    // Profile-owned native action and actor-value authority. Live activation
    // and contact transitions are wired separately; issuing an ID is not a hit.
    class OblivionCombatService
    {
        class PreparedNonPlayerView;
        ESM4::ActionLedger mActions;
        std::map<ESM::FormKey, ESM4::RuntimeActorValues> mActorValues;
        std::map<ESM::FormKey, ESM4::RuntimeActorBaseOverride> mActorBases;
        std::map<ESM::FormKey, ESM4::RuntimeActorLife> mActorLife;
        std::uint64_t mNextDeathEvent = 1;
        std::deque<ESM4::RuntimeActorDeathEvent> mPendingDeathEvents;
        std::map<ESM::FormKey, std::uint16_t> mDeathCounts;
        struct PreparedLifeTransition
        {
            std::deque<ESM4::RuntimeActorDeathEvent> mEvents;
            std::map<ESM::FormKey, std::uint16_t> mCounts;
        };
        const ESM4::RuntimeActorBaseOverride* findActorBase(const ESM::FormKey& base) const;
        std::optional<PreparedLifeTransition> prepareLifeTransition(
            const ESM4::RuntimeActorLife& life) const;
        bool enterNonPlayerDeath(const MWWorld::Ptr& actor, const ESM::FormKey& killer, bool essential,
            const ESM4::EssentialRecoverySettings& settings, bool healthGate);
        bool enterPlayerDeath(MWWorld::Player& player, const ESM::FormKey& killer, bool essential,
            const ESM4::EssentialRecoverySettings& settings, bool healthGate, bool godMode);
        void prepareEssentialWake(ESM4::RuntimeActorValues& values, ESM4::RuntimeActorLife& life,
            bool essential, bool godMode, const ESM4::EssentialRecoverySettings& settings) const;
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
        // Typed command writers. Set shares a base transaction; Mod and Force
        // select Script/console Damage storage and preserve native eligibility.
        // No callback can observe partially published state. The caller owns
        // notifications, health reactions and process-cache policy.
        OblivionActorValueCommandResult executeNonPlayerValueCommand(const MWWorld::Ptr& actor,
            std::uint8_t value, ESM4::ActorValueCommand command, ESM4::ActorValueCommandSource source,
            std::int32_t requested, const ESM4::ActorValueCommandPolicy& policy,
            std::span<const MWWorld::Ptr> residents);
        OblivionActorValueCommandResult executePlayerValueCommand(MWWorld::Player& player,
            std::uint8_t value, ESM4::ActorValueCommand command, ESM4::ActorValueCommandSource source,
            std::int32_t requested, const ESM4::ActorValueCommandPolicy& policy,
            const ESM4::PlayerDynamicBaseSettings& settings);
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
        // Script/condition GetAV differs on a disabled reference: the original
        // queries the raw base form, bypassing process and player derivation.
        // GetBaseAV always uses its separate floored resolved-base query.
        // The caller supplies reference enablement, independently of Health.
        double getScriptActorValue(const ESM::FormKey& actor, std::uint8_t value,
            bool base, bool disabled, const MWWorld::ESMStore& store) const;
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
        // Explicit construction/load publication, after content validation.
        // Changes lifecycle authority and shared views without issuing events.
        // Normal health/death/resurrection transitions have separate policy.
        void publishNonPlayerLife(const MWWorld::Ptr& actor, ESM4::RuntimeActorLife life);
        void publishPlayerLife(MWWorld::Player& player, ESM4::RuntimeActorLife life);
        // Caller resolves eligibility, essential policy and source identity.
        // Requires an initialized life entry. A phase change and its death
        // event commit together; same-phase requests preserve the first cause.
        bool transitionNonPlayerLife(const MWWorld::Ptr& actor, ESM4::RuntimeActorLife life);
        bool transitionPlayerLife(MWWorld::Player& player, ESM4::RuntimeActorLife life);
        // Negative-Health callback entry, after the value writer commits.
        // Caller supplies resolved essential eligibility and validated source.
        // Alive Health below one enters death or essential unconsciousness;
        // essential Health restoration and lifecycle views commit together.
        bool reactNonPlayerHealth(const MWWorld::Ptr& actor, const ESM::FormKey& killer, bool essential,
            const ESM4::EssentialRecoverySettings& settings);
        bool reactPlayerHealth(MWWorld::Player& player, const ESM::FormKey& killer, bool essential,
            const ESM4::EssentialRecoverySettings& settings, bool godMode = false);
        // Script Kill calls the same death entry without testing current Health.
        // Source validation and physical/script aftermath remain caller-owned.
        bool killNonPlayer(const MWWorld::Ptr& actor, const ESM::FormKey& killer, bool essential,
            const ESM4::EssentialRecoverySettings& settings);
        bool killPlayer(MWWorld::Player& player, const ESM::FormKey& killer, bool essential,
            const ESM4::EssentialRecoverySettings& settings, bool godMode = false);
        // AV/lifecycle portion of full resurrection, without a Health writer
        // callback or a new death event. Historical events/counts persist.
        // Nonplayer process resets to Low; Player recreates an active process.
        // World inventory/base/controller/3D reset and preserve-state selection
        // remain caller-owned and must complete before exposing script success.
        void resetNonPlayerForResurrection(const MWWorld::Ptr& actor);
        void resetPlayerForResurrection(MWWorld::Player& player);
        // Returns true when an eligible expired timer is processed, including
        // immediate reentry/death caused by the recovery Health callback.
        // Caller supplies the native process knocked-state byte and frame delta.
        bool advanceNonPlayerEssentialRecovery(const MWWorld::Ptr& actor, float frameSeconds,
            std::int8_t knockedState, bool essential, const ESM4::EssentialRecoverySettings& settings);
        bool advancePlayerEssentialRecovery(MWWorld::Player& player, float frameSeconds,
            std::int8_t knockedState, bool essential, const ESM4::EssentialRecoverySettings& settings,
            bool godMode = false);
        const ESM4::RuntimeActorLife* findActorLife(const ESM::FormKey& actor) const;
        // Pop before invoking the callback. Callback-triggered saves retain
        // remaining FIFO work and never replay the event being dispatched.
        std::optional<ESM4::RuntimeActorDeathEvent> takeNextDeathEvent();
        std::int32_t getDeadCount(const ESM::FormKey& base) const;
        void capture(ESM4::RuntimeState& state) const;
        void restore(const ESM4::RuntimeState& state);
        // World load preflight: validate winning native actor bindings before
        // replacing either action ownership or actor values.
        void restore(const ESM4::RuntimeState& state, const MWWorld::ESMStore& store);
    };
}

#endif
