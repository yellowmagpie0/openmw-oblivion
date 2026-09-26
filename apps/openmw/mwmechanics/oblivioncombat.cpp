#include "oblivioncombat.hpp"

#include <components/esm4/runtimestate.hpp>

#include <algorithm>
#include <stdexcept>
#include <type_traits>

#include <components/esm4/physicalcombat.hpp>
#include <components/esm4/loadnpc.hpp>
#include "../mwworld/class.hpp"
#include "../mwworld/ptr.hpp"

#include "creaturestats.hpp"
#include "npcstats.hpp"
#include "../mwworld/oblivionactorstats.hpp"

namespace MWMechanics
{
    namespace
    {
        void validateNpcIdentity(const MWWorld::Ptr& ptr, const ESM4::RuntimeActorValues& values)
        {
            if (ptr.isEmpty() || ptr.getType() != ESM::REC_NPC_4)
                throw std::invalid_argument("native NPC values require a TES4 NPC reference");
            const auto* base = ptr.get<ESM4::Npc>()->mBase;
            if (!base || !base->mIsTES4 || values.mOwner != ESM4::ActorValueOwner::NonPlayer
                || values.mActor != ptr.getCellRef().getFormKey() || values.mBase != base->mFormKey)
                throw std::invalid_argument("native NPC actor-value identity mismatch");
        }

        void validateNpcQuery(std::uint8_t value)
        {
            // Inventory Encumbrance and High-process Paralysis use additional
            // native state. Do not silently treat either as an ordinary scalar.
            if (value >= 72 || value == 11 || value == 48)
                throw std::invalid_argument("native NPC scalar query excludes inventory Encumbrance and process Paralysis");
        }

        float npcFloat(const ESM4::RuntimeActorValues& values, std::uint8_t value)
        {
            validateNpcQuery(value);
            const auto current = ESM4::composeActorValue(values.mValues[value], values.mOwner, values.mProcess);
            if (value != 9)
                return current;
            const auto multiplier = ESM4::composeActorValue(values.mValues[40], values.mOwner, values.mProcess);
            return ESM4::scaleNpcMagicka(current, multiplier);
        }

        OblivionActorProjectionInput npcProjection(const ESM4::RuntimeActorValues& values)
        {
            OblivionActorProjectionInput input;
            input.mOwner = values.mOwner;
            input.mProcess = values.mProcess;
            std::copy_n(values.mValues.begin(), 8, input.mAttributes.begin());
            std::copy_n(values.mValues.begin() + 12, 21, input.mSkills.begin());
            for (std::size_t i = 0; i < input.mDynamic.size(); ++i)
            {
                const auto& value = values.mValues[8 + i];
                input.mDynamic[i] = {value.mBase,
                    ESM4::dynamicActorValueMaximum(ESM4::combatBaseValue(value.mBase),
                        value.mModifiers[0].value_or(0.f), values.mOwner, values.mProcess),
                    npcFloat(values, static_cast<std::uint8_t>(8 + i))};
            }
            return input;
        }
    }

    OblivionActorProjection::OblivionActorProjection(
        CreatureStats& target, const OblivionActorProjectionInput& input)
        : OblivionActorProjection(target, nullptr, input)
    {
    }

    OblivionActorProjection::OblivionActorProjection(
        NpcStats& target, const OblivionActorProjectionInput& input)
        : OblivionActorProjection(target, &target, input)
    {
    }

    OblivionActorProjection::OblivionActorProjection(
        CreatureStats& target, NpcStats* npc, const OblivionActorProjectionInput& input)
        : mTarget(target)
        , mNpc(npc)
        , mAttributes(target.mAttributes)
    {
        for (std::size_t i = 0; i < input.mAttributes.size(); ++i)
            mAttributes.at(ESM::Attribute::indexToRefId(i)).setNativeProjection(
                input.mAttributes[i], input.mOwner, input.mProcess);
        for (std::size_t i = 0; i < input.mDynamic.size(); ++i)
            mDynamic[i].setNativeProjection(input.mDynamic[i][0], input.mDynamic[i][1], input.mDynamic[i][2]);
        if (npc)
        {
            mSkills = npc->mSkills;
            const auto& ids = MWWorld::oblivionSkillIds();
            for (std::size_t i = 0; i < ids.size(); ++i)
                mSkills.at(ids[i]).setNativeProjection(input.mSkills[i], input.mOwner, input.mProcess);
        }
    }

    bool OblivionActorProjection::commit() noexcept
    {
        if (mCommitted)
            return false;
        mTarget.mAttributes.swap(mAttributes);
        if (mNpc)
            mNpc->mSkills.swap(mSkills);
        for (std::size_t i = 0; i < mDynamic.size(); ++i)
        {
            // Bypass the deliberately guarded public assignment only here,
            // after complete preparation. These scalar copies cannot throw.
            mTarget.mDynamic[i].mStatic = mDynamic[i].mStatic;
            mTarget.mDynamic[i].mCurrent = mDynamic[i].mCurrent;
            mTarget.mDynamic[i].mNativeModified = mDynamic[i].mNativeModified;
        }
        mCommitted = true;
        return true;
    }

    void OblivionCombatService::clear()
    {
        mActions = {};
        mActorValues.clear();
    }

    std::uint64_t OblivionCombatService::allocateAction()
    {
        return mActions.allocate();
    }

    bool OblivionCombatService::isActionPending(std::uint64_t id) const
    {
        return mActions.isPending(id);
    }

    bool OblivionCombatService::isActionConsumed(std::uint64_t id) const
    {
        return mActions.isConsumed(id);
    }

    bool OblivionCombatService::consumeAction(std::uint64_t id)
    {
        return mActions.consume(id);
    }

    const ESM4::RuntimeActorValues& OblivionCombatService::npcValues(const MWWorld::Ptr& actor) const
    {
        if (actor.isEmpty())
            throw std::invalid_argument("native NPC actor-value lookup has no actor");
        const auto* values = findActorValues(actor.getCellRef().getFormKey());
        if (!values)
            throw std::invalid_argument("native NPC actor values have not been initialized");
        validateNpcIdentity(actor, *values);
        return *values;
    }

    void OblivionCombatService::publishNpcValues(const MWWorld::Ptr& actor, ESM4::RuntimeActorValues values)
    {
        values.validate();
        validateNpcIdentity(actor, values);
        const auto projection = npcProjection(values);
        OblivionActorProjection prepared(actor.getClass().getNpcStats(actor), projection);
        const auto found = mActorValues.find(values.mActor);
        if (found == mActorValues.end())
        {
            // Allocation can fail, but no views have changed yet.
            const auto key = values.mActor;
            mActorValues.emplace(key, std::move(values));
        }
        else
        {
            static_assert(std::is_nothrow_swappable_v<ESM4::RuntimeActorValues>);
            std::swap(found->second, values);
        }
        prepared.commit();
    }

    void OblivionCombatService::changeNpcValue(const MWWorld::Ptr& actor, std::uint8_t value,
        ESM4::ActorValueModifier modifier, float delta)
    {
        validateNpcQuery(value);
        auto candidate = npcValues(actor);
        candidate.mValues[value] = ESM4::changeActorValueModifier(
            candidate.mValues[value], candidate.mOwner, modifier, delta);
        publishNpcValues(actor, std::move(candidate));
    }

    float OblivionCombatService::getNpcValue(const MWWorld::Ptr& actor, std::uint8_t value) const
    {
        return npcFloat(npcValues(actor), value);
    }

    std::int32_t OblivionCombatService::getNpcIntegerValue(const MWWorld::Ptr& actor, std::uint8_t value) const
    {
        validateNpcQuery(value);
        const auto& values = npcValues(actor);
        const auto& state = values.mValues[value];
        const auto current = ESM4::composeIntegerActorValue(
            ESM4::combatBaseValue(state.mBase), state.mModifiers, values.mOwner, values.mProcess);
        if (value != 9)
            return current;
        const auto multiplier = ESM4::composeActorValue(values.mValues[40], values.mOwner, values.mProcess);
        return ESM4::scaleNpcIntegerMagicka(current, multiplier);
    }

    const ESM4::RuntimeActorValues* OblivionCombatService::findActorValues(const ESM::FormKey& actor) const
    {
        const auto found = mActorValues.find(actor);
        return found == mActorValues.end() ? nullptr : &found->second;
    }

    void OblivionCombatService::capture(ESM4::RuntimeState& state) const
    {
        if (state.mProfile != ESM::GameProfile::Oblivion || state.mVersion < 8
            || state.mVersion > ESM4::CurrentRuntimeStateVersion)
            throw std::invalid_argument("native physical actions require an Oblivion v8+ save");
        if (state.mVersion < 9 && !mActorValues.empty())
            throw std::invalid_argument("native actor values require an Oblivion v9+ save");
        std::vector<ESM4::RuntimeActorValues> actors;
        actors.reserve(mActorValues.size());
        for (const auto& [key, actor] : mActorValues)
            actors.push_back(actor);
        auto actions = mActions.capture();
        state.mNativeActorValues.swap(actors);
        state.mPhysicalActions = std::move(actions);
    }

    void OblivionCombatService::restore(const ESM4::RuntimeState& state)
    {
        state.validate();
        ESM4::ActionLedger actions;
        actions.restore(state.mPhysicalActions);
        std::map<ESM::FormKey, ESM4::RuntimeActorValues> actors;
        for (const auto& actor : state.mNativeActorValues)
            actors.emplace(actor.mActor, actor);
        mActions = std::move(actions);
        mActorValues.swap(actors);
    }
}
