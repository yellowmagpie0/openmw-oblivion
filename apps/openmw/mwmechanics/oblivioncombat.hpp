#ifndef OPENMW_MWMECHANICS_OBLIVIONCOMBAT_H
#define OPENMW_MWMECHANICS_OBLIVIONCOMBAT_H

#include <components/esm4/runtimestate.hpp>
#include <components/esm4/actorvalues.hpp>
#include <components/esm/refid.hpp>

#include "stat.hpp"

#include <array>
#include <map>

namespace MWWorld
{
    class Ptr;
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
        NpcStats* mNpc;
        std::map<ESM::RefId, AttributeValue> mAttributes;
        std::map<ESM::RefId, SkillValue> mSkills;
        std::array<DynamicStat<float>, 3> mDynamic;
        bool mCommitted = false;

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

    // Profile-owned native action and actor-value authority. Live activation
    // and contact transitions are wired separately; issuing an ID is not a hit.
    class OblivionCombatService
    {
        ESM4::ActionLedger mActions;
        std::map<ESM::FormKey, ESM4::RuntimeActorValues> mActorValues;
        const ESM4::RuntimeActorValues& npcValues(const MWWorld::Ptr& actor) const;

    public:
        void clear();
        std::uint64_t allocateAction();
        bool isActionPending(std::uint64_t id) const;
        bool isActionConsumed(std::uint64_t id) const;
        // Internal completion/cancellation boundary. Call only with the
        // corresponding gameplay transition committed, before event callbacks.
        bool consumeAction(std::uint64_t id);
        // Explicit construction/load publication. Only TES4 nonplayer NPCs
        // are supported here; automatic gameplay activation is wired separately.
        void publishNpcValues(const MWWorld::Ptr& actor, ESM4::RuntimeActorValues values);
        // Caller applies eligibility, event and death policy before/after this
        // scalar transition. This method cannot run callbacks between commits.
        void changeNpcValue(const MWWorld::Ptr& actor, std::uint8_t value,
            ESM4::ActorValueModifier modifier, float delta);
        float getNpcValue(const MWWorld::Ptr& actor, std::uint8_t value) const;
        std::int32_t getNpcIntegerValue(const MWWorld::Ptr& actor, std::uint8_t value) const;
        const ESM4::RuntimeActorValues* findActorValues(const ESM::FormKey& actor) const;
        void capture(ESM4::RuntimeState& state) const;
        void restore(const ESM4::RuntimeState& state);
    };
}

#endif
