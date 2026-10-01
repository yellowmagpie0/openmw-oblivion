#ifndef OPENMW_MWMECHANICS_OBLIVIONCOMBAT_H
#define OPENMW_MWMECHANICS_OBLIVIONCOMBAT_H

#include <components/esm4/runtimestate.hpp>
#include <components/esm4/actorvalues.hpp>
#include <components/esm4/physicalcombat.hpp>
#include <components/esm/refid.hpp>

#include "stat.hpp"

#include <array>
#include <map>
#include <memory>
#include <deque>
#include <span>
#include <set>
#include <string_view>

namespace ESM4
{
    struct ActorCharacterBaseStats;
    struct PassiveAbilityInput;
}

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
    class OblivionCombatService;

    // Keeps first-time lifecycle adoption inside a writer's transaction.
    // Prepare before adoption; commit only after the native writer succeeds.
    class OblivionActorLifeAdoption
    {
        friend class OblivionCombatService;
        struct Impl;
        std::unique_ptr<Impl> mImpl;
        explicit OblivionActorLifeAdoption(std::unique_ptr<Impl> impl);

    public:
        OblivionActorLifeAdoption();
        ~OblivionActorLifeAdoption();
        OblivionActorLifeAdoption(OblivionActorLifeAdoption&&) noexcept;
        OblivionActorLifeAdoption& operator=(OblivionActorLifeAdoption&&) noexcept;
        void commit() noexcept;
    };

    struct OblivionActorProjectionInput
    {
        ESM4::ActorValueOwner mOwner = ESM4::ActorValueOwner::NonPlayer;
        ESM4::ActorValueProcess mProcess = ESM4::ActorValueProcess::Active;
        std::array<ESM4::ActorValueState, 8> mAttributes{};
        std::array<ESM4::ActorValueState, 21> mSkills{};
        // Shared AI order: Hello, Fight, Flee, Alarm. Absence preserves
        // the target during staged projections that do not own AI yet.
        std::optional<std::array<ESM4::ActorValueState, 4>> mAiSettings;
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
        std::optional<std::array<Stat<int>, 4>> mAiSettings;
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

    struct OblivionActorMovement
    {
        std::int32_t mEncumbrance;
        bool mRunning;
        bool mCanSpend;
    };

    struct OblivionFrameSettings
    {
        OblivionFatigueSettings mFatigue;
        ESM4::MagickaRegenerationSettings mMagicka;
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

    struct OblivionActorRestoration
    {
        ESM::FormKey mActor;
        std::vector<OblivionRestorationUpdate> mUpdates;
    };

    struct OblivionActorValueCommandResult
    {
        // False means eligibility suppressed both storage and notifications.
        bool mAccepted;
        // Negative-Health reaction has committed with storage; this is notification metadata.
        std::optional<float> mHealthReactionDelta;
    };

    struct OblivionBreathUpdateResult
    {
        float mRemaining;
        float mMaximum;
        float mDamage;
        bool mDrowning;
    };

    // Signed native Damage-channel deltas, already resolved by contact policy.
    // Geometry, mitigation, equipment wear and reactions are caller contracts.
    struct OblivionPhysicalContactDeltas
    {
        float mAttackerFatigue = 0;
        float mVictimHealth = 0;
        float mVictimFatigue = 0;
    };

    struct OblivionPassiveEffectIdentity
    {
        ESM::FormKey mSpell;
        std::uint32_t mEffectIndex;
    };

    // Profile-owned native action and actor-value authority. Live activation
    // and contact transitions are wired separately; issuing an ID is not a hit.
    class OblivionCombatService
    {
        friend class OblivionActorLifeAdoption;
        class PreparedNonPlayerView;
        ESM4::ActionLedger mActions;
        std::map<std::uint64_t, ESM::FormKey> mActionOwners;
        std::map<ESM::FormKey, ESM4::RuntimeMeleeState> mMeleeStates;
        std::map<ESM::FormKey, float> mAnimationClocks;
        void consumeContactAction(std::uint64_t id, const ESM::FormKey& actor) noexcept;
        std::map<ESM::FormKey, ESM4::RuntimeActorValues> mActorValues;
        std::map<ESM::FormKey, ESM4::RuntimeActorBaseOverride> mActorBases;
        std::map<ESM::FormKey, ESM4::RuntimeActorLife> mActorLife;
        std::uint64_t mNextDeathEvent = 1;
        std::deque<ESM4::RuntimeActorDeathEvent> mPendingDeathEvents;
        std::map<ESM::FormKey, std::uint16_t> mDeathCounts;
        std::map<ESM::FormKey, float> mActorBreath;
        float mActorManagerTime = 0;
        std::map<ESM::FormKey, float> mActorUpdateTimes;
        std::map<ESM::FormKey, std::set<ESM::FormKey>> mCombatOpponents;
        struct PreparedLifeTransition
        {
            std::deque<ESM4::RuntimeActorDeathEvent> mEvents;
            std::map<ESM::FormKey, std::uint16_t> mCounts;
        };
        const ESM4::RuntimeActorBaseOverride* findActorBase(const ESM::FormKey& base) const;
        std::optional<PreparedLifeTransition> prepareLifeTransition(
            const ESM4::RuntimeActorLife& life, bool afterRevival = false) const;
        bool enterNonPlayerDeath(const MWWorld::Ptr& actor, const ESM::FormKey& killer, bool essential,
            const ESM4::EssentialRecoverySettings& settings, bool healthGate);
        bool enterPlayerDeath(MWWorld::Player& player, const ESM::FormKey& killer, bool essential,
            const ESM4::EssentialRecoverySettings& settings, bool healthGate, bool godMode);
        void prepareEssentialWake(ESM4::RuntimeActorValues& values, ESM4::RuntimeActorLife& life,
            bool essential, bool godMode, const ESM4::EssentialRecoverySettings& settings) const;
        void preparePlayerValues(ESM4::RuntimeActorValues& values,
            const ESM4::PlayerDynamicBaseSettings& settings) const;
        void setNonPlayerBaseValue(const ESM4::RuntimeActorValues& target, ESM4::ActorBaseKind kind,
            std::uint8_t value, std::int32_t requested, std::span<const MWWorld::Ptr> residents);
        ESM4::RuntimeActorLife prepareHealthReaction(ESM4::RuntimeActorValues& values,
            bool essential, const ESM4::EssentialRecoverySettings& settings, bool godMode,
            const ESM::FormKey& source) const;
        void publishHealthChange(const MWWorld::Ptr& actor, ESM4::RuntimeActorValues values,
            bool essential, const ESM4::EssentialRecoverySettings& settings, bool godMode,
            const ESM::FormKey& source = {});
        std::optional<OblivionBreathUpdateResult> prepareBreathUpdate(const ESM4::RuntimeActorValues& values,
            float duration, bool needsAir, const ESM4::SwimBreathSettings& settings) const;
        void publishBreathUpdate(const MWWorld::Ptr& actor, MWWorld::Player* player,
            const ESM::FormKey& key, const OblivionBreathUpdateResult& update, bool essential,
            const ESM4::EssentialRecoverySettings& recovery,
            const ESM4::PlayerDynamicBaseSettings& playerBase);
        ESM4::RuntimeActorValues preparePlayerInitialization(MWWorld::Player& player,
            const MWWorld::ESMStore& store, std::optional<bool> legacyDead);
        void preparePlayerCharacterReplacement(ESM4::RuntimeActorValues& values,
            const ESM4::ActorCharacterBaseStats& stats, std::span<const ESM4::PassiveAbilityInput> abilities,
            std::span<const OblivionPassiveEffectIdentity> removalOrder,
            const ESM4::PlayerDynamicBaseSettings& settings, bool essential,
            const ESM4::EssentialRecoverySettings& recovery, bool godMode);
        void preparePlayerCharacterBase(ESM4::RuntimeActorValues& values,
            const ESM4::ActorCharacterBaseStats& stats, const ESM4::PlayerDynamicBaseSettings& settings);
        bool preparePlayerPassiveGrants(ESM4::RuntimeActorValues& values,
            std::span<const ESM4::PassiveAbilityInput> abilities,
            const ESM4::PlayerDynamicBaseSettings& settings, bool essential,
            const ESM4::EssentialRecoverySettings& recovery, bool godMode);
        bool preparePlayerPassiveRemoval(ESM4::RuntimeActorValues& values,
            const ESM::FormKey& spell, std::uint32_t effectIndex,
            const ESM4::PlayerDynamicBaseSettings& settings, bool godMode);
        void commitPreparedPlayer(MWWorld::Player& player, OblivionCombatService& prepared,
            ESM4::RuntimeActorValues values, const ESM4::PlayerDynamicBaseSettings& settings);
        const ESM4::RuntimeActorValues& playerValues() const;
        const ESM4::RuntimeActorValues& nonPlayerValues(const MWWorld::Ptr& actor) const;
        const ESM4::RuntimeActorValues& nonPlayerValues(const ESM::FormKey& actor) const;

    public:
        OblivionActorLifeAdoption guardLifeAdoption(const MWWorld::Ptr& actor, MWWorld::Player* player = nullptr);
        void clear();
        float actorManagerTime() const noexcept { return mActorManagerTime; }
        void advanceFrameClock(float duration);
        float elapsedSinceActorUpdate(const ESM::FormKey& actor, float time) const;
        // Membership authority only: callers own attack/AI/legal eligibility.
        // Endpoints require native values and nonterminal lifecycle. Repeated
        // engagement is idempotent; stop/death removes both sides without events.
        bool engage(const ESM::FormKey& actor, const ESM::FormKey& opponent);
        bool stopCombat(const ESM::FormKey& actor) noexcept;
        bool isInCombat(const ESM::FormKey& actor) const;
        bool isInCombatWith(const ESM::FormKey& actor, const ESM::FormKey& opponent) const;
        std::vector<ESM::FormKey> combatOpponents(const ESM::FormKey& actor) const;
        // Controller state has the same native actor/action authority as hits.
        // These methods do not choose groups, advance animation or resolve damage.
        const ESM4::RuntimeMeleeState* findMeleeState(const ESM::FormKey& actor) const;
        void setMeleeInput(const ESM::FormKey& actor, const ESM4::RuntimeMeleeInput& input);
        float animationClock(const ESM::FormKey& actor) const;
        float advanceAnimationClock(const ESM::FormKey& actor, float duration);
        bool setMeleeSequenceTiming(std::uint64_t id, const ESM::FormKey& actor,
            const ESM4::MeleeSequenceTiming& timing);
        std::uint64_t beginMeleeStrike(const ESM::FormKey& actor, ESM4::MeleeStrikeKind kind,
            std::string_view animationGroup, float playbackSpeed = 1, const ESM::FormKey& weaponBase = {});
        bool updateMeleeAnimation(std::uint64_t id, const ESM::FormKey& actor, float time);
        // One original ordinary phase step per caller animation update. No
        // inferred contact or renderer clock mutation occurs here.
        bool advanceOrdinaryMeleePhase(std::uint64_t id, const ESM::FormKey& actor,
            float sequenceOffset, float animationClock, const std::array<float, 4>& keyTimes);
        // Publish clock, ordinary phase and sequence timing together after all
        // validation. Requires initialized timing; no contact/render callbacks.
        bool advanceOrdinaryMeleeSequence(std::uint64_t id, const ESM::FormKey& actor,
            float duration, float frequency, float begin, float end,
            const std::array<float, 4>& keyTimes, bool freezeClock = false);
        // Read the prior animation phase before its next frame update.
        bool isOrdinaryMeleeContactPending(std::uint64_t id, const ESM::FormKey& actor) const;
        bool finishMeleeStrike(std::uint64_t id, const ESM::FormKey& actor);
        // Hard interruption clears held/queued input as well as the owned strike.
        // It is distinct from normal animation completion, which retains queues.
        bool cancelMeleeStrike(std::uint64_t id, const ESM::FormKey& actor);
        // No allocation or Alive precondition: safe after death/life cancellation.
        void clearMeleeInput(const ESM::FormKey& actor) noexcept;
        std::uint64_t allocateAction();
        // An owned intent requires initialized Alive authority. Allocation
        // publishes ID and owner together; animation handles are never stored.
        std::uint64_t allocateAction(const ESM::FormKey& actor);
        bool isActionPending(std::uint64_t id, const ESM::FormKey& actor) const;
        bool consumeAction(std::uint64_t id, const ESM::FormKey& actor);
        std::size_t cancelActorActions(const ESM::FormKey& actor) noexcept;
        // False for stale/wrong-owner or terminal contacts. Validate and prepare
        // both actors, projections and death events before changing any live
        // state. Resource publication and consumption precede external callbacks.
        // An empty victim commits a swing/miss with no victim deltas.
        bool commitPhysicalContact(std::uint64_t id, const MWWorld::Ptr& attacker,
            const MWWorld::Ptr& victim, const OblivionPhysicalContactDeltas& deltas,
            MWWorld::Player* player, bool victimEssential,
            const ESM4::EssentialRecoverySettings& recovery,
            const ESM4::PlayerDynamicBaseSettings& playerBase, bool playerGodMode = false);
        bool isActionPending(std::uint64_t id) const;
        bool isActionConsumed(std::uint64_t id) const;
        // Internal completion/cancellation boundary. Call only with the
        // corresponding gameplay transition committed, before event callbacks.
        bool consumeAction(std::uint64_t id);
        // Explicit construction/load publication. TES4 NPCs and creatures
        // are supported here; automatic gameplay activation is wired separately.
        void publishNonPlayerValues(const MWWorld::Ptr& actor, ESM4::RuntimeActorValues values);
        // Prepare first values/life together, or reproject restored authority.
        // Fresh process/level inputs never overwrite an existing snapshot.
        // Scene activation may explicitly change its process while preserving
        // values, modifiers and lifecycle; projection validates before commit.
        // Legacy death adoption generates no historical events or death counts.
        void initializeNonPlayerActor(const MWWorld::Ptr& actor, const MWWorld::ESMStore& store,
            std::optional<std::uint16_t> playerLevel, ESM4::ActorValueProcess process,
            std::optional<bool> legacyDead, bool activate = false);
        // Caller applies eligibility, event and death policy before/after this
        // scalar transition. This method cannot run callbacks between commits.
        void changeNonPlayerValue(const MWWorld::Ptr& actor, std::uint8_t value,
            ESM4::ActorValueModifier modifier, float delta);
        // Lua attribute/skill requests select one native modifier channel;
        // they preserve Base/Script and use the native float delta writer.
        void requestNonPlayerStatModifier(const MWWorld::Ptr& actor, std::uint8_t value,
            ESM4::ActorValueModifier modifier, float requested);
        void requestPlayerStatModifier(MWWorld::Player& player, std::uint8_t value,
            ESM4::ActorValueModifier modifier, float requested,
            const ESM4::PlayerDynamicBaseSettings& settings);
        // Native float Damage-channel writes, including fractional frame costs.
        // Negative deltas commit their lifecycle reaction and source attribution
        // atomically; positive/zero writes preserve life. False means god mode
        // suppressed a Player debit. The caller validates the source actor.
        bool changeNonPlayerHealth(const MWWorld::Ptr& actor, float delta, const ESM::FormKey& source,
            bool essential, const ESM4::EssentialRecoverySettings& settings);
        bool changePlayerHealth(MWWorld::Player& player, float delta, const ESM::FormKey& source,
            bool essential, const ESM4::EssentialRecoverySettings& settings,
            const ESM4::PlayerDynamicBaseSettings& baseSettings, bool godMode = false);
        // Resource-current requests (including Lua) debit/restore Damage without
        // replacing Maximum or Script. Native Damage clamps still apply; a
        // request above the recoverable pool need not equal the resulting value.
        // NPC Magicka requests are expressed in the scaled public units.
        bool requestNonPlayerResourceCurrent(const MWWorld::Ptr& actor, std::uint8_t value,
            float requested, const ESM4::ActorValueCommandPolicy& policy, bool essential,
            const ESM4::EssentialRecoverySettings& recovery);
        bool requestPlayerResourceCurrent(MWWorld::Player& player, std::uint8_t value,
            float requested, bool godMode, bool essential,
            const ESM4::EssentialRecoverySettings& recovery, const ESM4::PlayerDynamicBaseSettings& settings);
        // Water/controller eligibility is caller-owned. Timer and resulting
        // Health/lifecycle effects commit together. Dead actors return nullopt.
        std::optional<OblivionBreathUpdateResult> updateNonPlayerBreath(const MWWorld::Ptr& actor,
            float duration, bool needsAir, bool essential, const ESM4::SwimBreathSettings& settings,
            const ESM4::EssentialRecoverySettings& recovery);
        std::optional<OblivionBreathUpdateResult> updatePlayerBreath(MWWorld::Player& player,
            float duration, bool needsAir, bool essential, const ESM4::SwimBreathSettings& settings,
            const ESM4::EssentialRecoverySettings& recovery,
            const ESM4::PlayerDynamicBaseSettings& playerBase, bool godMode = false);
        // Shared base-record transaction. The caller supplies every resident,
        // published reference of this base, including actor. Unloaded saved
        // actors change too; newly published references inherit the override.
        // No pointers survive the call. Event/cache policy remains caller-owned.
        void setNonPlayerBaseValue(const MWWorld::Ptr& actor, std::uint8_t value,
            std::int32_t requested, std::span<const MWWorld::Ptr> residents);
        // Batch native attribute/skill form writes. Resource form inputs,
        // modifier containers, other base entries and lifecycle stay owned by
        // their existing authorities; no healing or ability application occurs.
        void publishPlayerCharacterBase(MWWorld::Player& player, const ESM4::ActorCharacterBaseStats& stats,
            const ESM4::PlayerDynamicBaseSettings& settings);
        // Scalar/lifecycle transaction only. The caller resolves winning
        // inputs and supplies the complete native active-list removal order;
        // race/class/birthsign metadata has a separate prepared publication.
        void replacePlayerCharacter(MWWorld::Player& player, const ESM4::ActorCharacterBaseStats& stats,
            std::span<const ESM4::PassiveAbilityInput> abilities,
            std::span<const OblivionPassiveEffectIdentity> removalOrder,
            const ESM4::PlayerDynamicBaseSettings& settings, bool essential = false,
            const ESM4::EssentialRecoverySettings& recovery = {}, bool godMode = false);
        void setPlayerBaseValue(MWWorld::Player& player, std::uint8_t value,
            std::int32_t requested, const ESM4::PlayerDynamicBaseSettings& settings);
        // Typed command writers. Set shares a base transaction; Mod and Force
        // select Script/console Damage storage and preserve native eligibility.
        // Negative Health storage and its lifecycle reaction commit together.
        // Health reactions require initialized lifecycle authority. The caller
        // owns external notifications and process-cache policy.
        OblivionActorValueCommandResult executeNonPlayerValueCommand(const MWWorld::Ptr& actor,
            std::uint8_t value, ESM4::ActorValueCommand command, ESM4::ActorValueCommandSource source,
            std::int32_t requested, const ESM4::ActorValueCommandPolicy& policy,
            std::span<const MWWorld::Ptr> residents, bool essential = false,
            const ESM4::EssentialRecoverySettings& recoverySettings = {});
        // Stable-key command path for an actor without a resident projection.
        // Winning reference/base bindings are validated before mutation. Set
        // still requires every resident sibling sharing the target base.
        OblivionActorValueCommandResult executeUnloadedValueCommand(const ESM::FormKey& actor,
            const MWWorld::ESMStore& store, std::uint8_t value, ESM4::ActorValueCommand command,
            ESM4::ActorValueCommandSource source, std::int32_t requested,
            const ESM4::ActorValueCommandPolicy& policy, std::span<const MWWorld::Ptr> residents,
            bool essential = false, const ESM4::EssentialRecoverySettings& recoverySettings = {});
        OblivionActorValueCommandResult executePlayerValueCommand(MWWorld::Player& player,
            std::uint8_t value, ESM4::ActorValueCommand command, ESM4::ActorValueCommandSource source,
            std::int32_t requested, const ESM4::ActorValueCommandPolicy& policy,
            const ESM4::PlayerDynamicBaseSettings& settings, bool essential = false,
            const ESM4::EssentialRecoverySettings& recoverySettings = {});
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
        // Adopt values and lifecycle together without death events/counts.
        // Restored authority takes precedence over fresh authored inputs.
        // Character generation and abilities are separate operations.
        void initializePlayerActor(MWWorld::Player& player, const MWWorld::ESMStore& store,
            std::optional<bool> legacyDead = {});
        // Bounded legacy-facade conversion is supplied explicitly by World.
        // Stage its values, breath and life together; never overwrite authority
        // or create historical death events. Missing passive history stays unknown.
        void initializePlayerActorFromLegacyView(MWWorld::Player& player, const MWWorld::ESMStore& store,
            ESM4::RuntimeActorValues values, float breathRemaining, std::optional<bool> legacyDead = {});
        // Stage constructor adoption and character replacement together. A
        // rejected choice publishes neither fresh authority nor its facade.
        void initializePlayerCharacter(MWWorld::Player& player, const MWWorld::ESMStore& store,
            const ESM4::ActorCharacterBaseStats& stats, std::span<const ESM4::PassiveAbilityInput> abilities,
            std::span<const OblivionPassiveEffectIdentity> removalOrder,
            const ESM4::PlayerDynamicBaseSettings& settings, bool essential = false,
            const ESM4::EssentialRecoverySettings& recovery = {}, bool godMode = false,
            std::optional<bool> legacyDead = {});
        // Narrow self passive grants. Known saved ownership prevents duplicate
        // application. Resolve winning spell/definition admission beforehand.
        void grantPlayerPassiveAbilities(MWWorld::Player& player,
            std::span<const ESM4::PassiveAbilityInput> abilities,
            const ESM4::PlayerDynamicBaseSettings& settings, bool essential = false,
            const ESM4::EssentialRecoverySettings& recovery = {});
        // Remove one identified applied effect using its saved magnitude. The
        // caller owns native active-list order; application order is not a
        // substitute for the original display-sorted removal traversal.
        void removePlayerPassiveEffect(MWWorld::Player& player, const ESM::FormKey& spell,
            std::uint32_t effectIndex, const ESM4::PlayerDynamicBaseSettings& settings, bool godMode = false);
        void changePlayerValue(MWWorld::Player& player, std::uint8_t value,
            ESM4::ActorValueModifier modifier, float delta, const ESM4::PlayerDynamicBaseSettings& settings);
        // Regeneration restores the native Damage channel. It must not clamp
        // the result through a TES3 DynamicStat or discard sparse presence.
        void regenerateNonPlayerFatigue(const MWWorld::Ptr& actor, float duration,
            const ESM4::FatigueRegenerationSettings& settings);
        void regeneratePlayerFatigue(MWWorld::Player& player, float duration,
            const ESM4::FatigueRegenerationSettings& settings, const ESM4::PlayerDynamicBaseSettings& baseSettings);
        // Frame Magicka is independent of Health restoration and Fatigue updates.
        // The caller supplies active casting-item state, not selected-spell state.
        void regenerateNonPlayerMagicka(const MWWorld::Ptr& actor, float duration,
            bool hasActiveMagicItem, const ESM4::MagickaRegenerationSettings& settings);
        void regeneratePlayerMagicka(MWWorld::Player& player, float duration,
            bool hasActiveMagicItem, const ESM4::MagickaRegenerationSettings& settings,
            const ESM4::PlayerDynamicBaseSettings& baseSettings);
        // Prepare Health, Magicka, then Fatigue and publish once. Callers own
        // rest/wait/jail eligibility and active-item/effect lifecycle.
        void restoreNonPlayerResources(const MWWorld::Ptr& actor, const OblivionRestorationUpdate& input,
            const OblivionRestorationSettings& settings);
        void restorePlayerResources(MWWorld::Player& player, const OblivionRestorationUpdate& input,
            const OblivionRestorationSettings& settings);
        // Atomic ordered resource updates across resident and unloaded actors.
        // Caller supplies every affected resident nonplayer; no pointers survive.
        // All actors require initialized lifecycle. Dead actors remain unchanged.
        // An optional completed clock commits with all resources/timestamps.
        // Caller owns processing eligibility and effect advancement.
        void restoreResourceBatch(MWWorld::Player& player, std::span<const OblivionActorRestoration> updates,
            std::span<const MWWorld::Ptr> residents, const OblivionRestorationSettings& settings,
            std::optional<float> completedManagerTime = {});
        // Running expenditure precedes regeneration; prepare and publish both
        // as one transition so failure cannot leave a partially updated actor.
        void updateNonPlayerFatigue(const MWWorld::Ptr& actor, const OblivionFatigueUpdate& input,
            const OblivionFatigueSettings& settings);
        void updatePlayerFatigue(MWWorld::Player& player, const OblivionFatigueUpdate& input,
            const OblivionFatigueSettings& settings);
        // A frame prepares Magicka and movement/Fatigue before publishing either.
        void updateNonPlayerFrameResources(const MWWorld::Ptr& actor, const OblivionFatigueUpdate& input,
            bool hasActiveMagicItem, const OblivionFrameSettings& settings);
        void updatePlayerFrameResources(MWWorld::Player& player, const OblivionFatigueUpdate& input,
            bool hasActiveMagicItem, const OblivionFrameSettings& settings);
        // Resource values and the last completed update time commit together.
        // Dead actors advance only their timestamp, preventing replay on revival.
        void updateNonPlayerFrameResourcesFromClock(const MWWorld::Ptr& actor, const OblivionActorMovement& movement,
            bool hasActiveMagicItem, const OblivionFrameSettings& settings);
        void updatePlayerFrameResourcesFromClock(MWWorld::Player& player, const OblivionActorMovement& movement,
            bool hasActiveMagicItem, const OblivionFrameSettings& settings);
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
        // Preserve-state resurrection changes Health through Damage after an
        // intermediate Alive phase. Caller has verified body/cell eligibility
        // and owns the native knocked-state/controller continuation.
        void reviveNonPlayerPreservingState(const MWWorld::Ptr& actor, bool essential,
            const ESM4::EssentialRecoverySettings& settings);
        // Returns true when an eligible expired timer is processed, including
        // immediate reentry/death caused by the recovery Health callback.
        // Caller supplies the native process knocked-state byte and frame delta.
        bool advanceNonPlayerEssentialRecovery(const MWWorld::Ptr& actor, float frameSeconds,
            std::int8_t knockedState, bool essential, const ESM4::EssentialRecoverySettings& settings);
        bool advancePlayerEssentialRecovery(MWWorld::Player& player, float frameSeconds,
            std::int8_t knockedState, bool essential, const ESM4::EssentialRecoverySettings& settings,
            bool godMode = false);
        // Native process byte, independent of shared animation/recoil flags.
        // Low processes return zero and ignore writes. Unknown Active legacy
        // state is diagnosed until a real native process transition supplies it.
        // Raw High action authority. Low getter returns -1; unknown legacy
        // Active state is diagnosed. Selection/render scheduling is separate.
        std::int16_t getProcessAction(const ESM::FormKey& actor) const;
        void setProcessAction(const ESM::FormKey& actor, std::int16_t action);
        std::int8_t getProcessKnockedState(const ESM::FormKey& actor) const;
        void setProcessKnockedState(const ESM::FormKey& actor, std::int8_t state);
        const ESM4::RuntimeActorLife* findActorLife(const ESM::FormKey& actor) const;
        std::optional<float> findActorBreath(const ESM::FormKey& actor) const;
        // Pop before invoking the callback. Callback-triggered saves retain
        // remaining FIFO work and never replay the event being dispatched.
        std::optional<ESM4::RuntimeActorDeathEvent> takeNextDeathEvent();
        std::int32_t getDeadCount(const ESM::FormKey& base) const;
        void capture(ESM4::RuntimeState& state) const;
        void restore(const ESM4::RuntimeState& state);
        // World load preflight: validate winning native actor bindings before
        // replacing either action ownership or actor values.
        void restore(const ESM4::RuntimeState& state, const MWWorld::ESMStore& store);
        // Projection requires the canonical Player aliases and raw form inputs.
        // World checks this on detached authority before changing world data.
        void validateRestoredPlayerBinding() const;
        // Install a validated replacement with Player and resident actor views.
        // Prepare every view before changing authority or committing projections.
        // Actors absent from replacement authority are not initialized here.
        void installRestoredActorState(OblivionCombatService&& replacement,
            std::span<const MWWorld::Ptr> residents, MWWorld::Player* player = nullptr);

    };
}

#endif
