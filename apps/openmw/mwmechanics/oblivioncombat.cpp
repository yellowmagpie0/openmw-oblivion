#include "oblivioncombat.hpp"

#include <components/esm4/runtimestate.hpp>

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <optional>

#include "../mwclass/esm4interactive.hpp"

#include <components/esm4/physicalcombat.hpp>
#include <components/esm4/loadnpc.hpp>
#include "../mwworld/class.hpp"
#include "../mwworld/ptr.hpp"
#include "../mwworld/player.hpp"
#include "../mwworld/esmstore.hpp"
#include <components/esm4/loadachr.hpp>

#include "creaturestats.hpp"
#include "npcstats.hpp"
#include "../mwworld/oblivionactorstats.hpp"

namespace MWMechanics
{
    namespace
    {
        void validateNonPlayerIdentity(const MWWorld::Ptr& ptr, const ESM4::RuntimeActorValues& values)
        {
            if (ptr.isEmpty())
                throw std::invalid_argument("native nonplayer values require a TES4 actor reference");
            ESM::FormKey baseKey;
            if (ptr.getType() == ESM::REC_NPC_4)
            {
                const auto* base = ptr.get<ESM4::Npc>()->mBase;
                if (base && base->mIsTES4)
                    baseKey = base->mFormKey;
            }
            else if (ptr.getType() == ESM::REC_CREA4)
            {
                const auto* base = ptr.get<ESM4::Creature>()->mBase;
                if (base && base->mAttackReach)
                    baseKey = base->mFormKey;
            }
            if (baseKey.isNull() || values.mOwner != ESM4::ActorValueOwner::NonPlayer
                || values.mActor != ptr.getCellRef().getFormKey() || values.mBase != baseKey)
                throw std::invalid_argument("native nonplayer actor-value identity mismatch");
        }

        bool nonPlayerContentIsCreature(const ESM4::RuntimeActorValues& values, const MWWorld::ESMStore& store)
        {
            if (values.mOwner != ESM4::ActorValueOwner::NonPlayer)
                throw std::invalid_argument("native nonplayer content lookup has player ownership");
            const auto* npc = store.search<ESM4::Npc>(values.mBase);
            const auto* creature = store.search<ESM4::Creature>(values.mBase);
            if ((!npc && !creature) || (npc && creature)
                || (npc && (!npc->mIsTES4 || npc->mFormKey != values.mBase))
                || (creature && (!creature->mAttackReach || creature->mFormKey != values.mBase)))
                throw std::invalid_argument("missing, ambiguous or unsupported native actor base: " + values.mBase.serialize());
            if (values.mActor.isContent())
            {
                const auto* characterRef = store.search<ESM4::ActorCharacter>(values.mActor);
                const auto* creatureRef = store.search<ESM4::ActorCreature>(values.mActor);
                const ESM4::ActorCharacter* reference = creature ? creatureRef : characterRef;
                if (!reference || (characterRef && creatureRef) || reference->mFormKey != values.mActor
                    || reference->mBaseKey != values.mBase)
                    throw std::invalid_argument("missing or mismatched native actor reference: " + values.mActor.serialize());
            }
            return creature != nullptr;
        }

        void validateNonPlayerQuery(std::uint8_t value)
        {
            // Inventory Encumbrance and High-process Paralysis use additional
            // native state. Do not silently treat either as an ordinary scalar.
            if (value >= 72 || value == 11 || value == 48)
                throw std::invalid_argument("native nonplayer scalar query excludes inventory Encumbrance and process Paralysis");
        }

        std::uint8_t nonPlayerValueIndex(bool creature, std::uint8_t value)
        {
            validateNonPlayerQuery(value);
            if (!creature)
                return value;
            // Original Creature runtime getter AND modifier wrappers alias
            // Marksman to Combat, unlike the base-form skill-group lookup.
            if ((value >= 12 && value <= 18) || value == 28)
                return 12;
            if (value >= 19 && value <= 25)
                return 19;
            return value >= 26 && value <= 32 ? 26 : value;
        }

        float nonPlayerFloat(const ESM4::RuntimeActorValues& values, std::uint8_t value)
        {
            validateNonPlayerQuery(value);
            const auto current = ESM4::composeActorValue(values.mValues[value], values.mOwner, values.mProcess);
            if (value != 9)
                return current;
            const auto multiplier = ESM4::composeActorValue(values.mValues[40], values.mOwner, values.mProcess);
            return ESM4::scaleNpcMagicka(current, multiplier);
        }

        std::int32_t nonPlayerInteger(const ESM4::RuntimeActorValues& values, std::uint8_t value)
        {
            validateNonPlayerQuery(value);
            const auto& state = values.mValues[value];
            const auto current = ESM4::composeIntegerActorValue(
                ESM4::combatBaseValue(state.mBase), state.mModifiers, values.mOwner, values.mProcess);
            if (value != 9)
                return current;
            const auto multiplier = ESM4::composeActorValue(values.mValues[40], values.mOwner, values.mProcess);
            return ESM4::scaleNpcIntegerMagicka(current, multiplier);
        }

        void validatePlayerIdentity(const ESM4::RuntimeActorValues& values)
        {
            if (values.mOwner != ESM4::ActorValueOwner::Player
                || values.mActor != ESM::FormKey::dynamic("player", 1)
                || values.mBase != ESM::FormKey::dynamic("player-base", 1)
                || !values.mPlayerFormValues)
                throw std::invalid_argument("native player values require player aliases and raw form inputs");
        }

        void validatePlayerQuery(std::uint8_t value)
        {
            if (value >= 72 || value == 11)
                throw std::invalid_argument("native player scalar query excludes inventory Encumbrance");
        }

        float fatigueRestoration(const ESM4::RuntimeActorValues& values, float duration,
            const ESM4::FatigueRegenerationSettings& settings)
        {
            const auto& fatigue = values.mValues[10];
            const auto& endurance = values.mValues[5];
            const float maximumModifier = values.mOwner == ESM4::ActorValueOwner::Player
                || values.mProcess == ESM4::ActorValueProcess::Active
                ? fatigue.mModifiers[0].value_or(0.f) : 0.f;
            return ESM4::fatigueRegeneration({
                ESM4::composeActorValue(fatigue, values.mOwner, values.mProcess), fatigue.mBase, maximumModifier,
                ESM4::composeIntegerActorValue(ESM4::combatBaseValue(endurance.mBase), endurance.mModifiers,
                    values.mOwner, values.mProcess), duration}, settings);
        }

        OblivionActorProjectionInput actorProjection(const ESM4::RuntimeActorValues& values)
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
                    values.mOwner == ESM4::ActorValueOwner::Player
                        ? ESM4::composeActorValue(value, values.mOwner, values.mProcess)
                        : nonPlayerFloat(values, static_cast<std::uint8_t>(8 + i))};
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
    {
        for (std::size_t i = 0; i < input.mAttributes.size(); ++i)
        {
            mAttributeTargets[i] = &target.mAttributes.at(ESM::Attribute::indexToRefId(i));
            mAttributes[i].setNativeProjection(input.mAttributes[i], input.mOwner, input.mProcess);
        }
        for (std::size_t i = 0; i < input.mDynamic.size(); ++i)
            mDynamic[i].setNativeProjection(input.mDynamic[i][0], input.mDynamic[i][1], input.mDynamic[i][2]);
        if (npc)
        {
            const auto& ids = MWWorld::oblivionSkillIds();
            for (std::size_t i = 0; i < ids.size(); ++i)
            {
                mSkillTargets[i] = &npc->mSkills.at(ids[i]);
                mSkills[i].setNativeProjection(input.mSkills[i], input.mOwner, input.mProcess);
            }
        }
    }

    void OblivionActorProjection::replaceAttribute(AttributeValue& target, const AttributeValue& value) noexcept
    {
        target.mBase = value.mBase;
        target.mModifier = value.mModifier;
        target.mDamage = value.mDamage;
        target.mNativeCurrent = value.mNativeCurrent;
    }

    bool OblivionActorProjection::commit() noexcept
    {
        if (mCommitted)
            return false;
        for (std::size_t i = 0; i < mAttributes.size(); ++i)
            replaceAttribute(*mAttributeTargets[i], mAttributes[i]);
        for (std::size_t i = 0; i < mSkills.size(); ++i)
            if (mSkillTargets[i])
                replaceAttribute(*mSkillTargets[i], mSkills[i]);
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

    const ESM4::RuntimeActorValues& OblivionCombatService::nonPlayerValues(const MWWorld::Ptr& actor) const
    {
        if (actor.isEmpty())
            throw std::invalid_argument("native nonplayer actor-value lookup has no actor");
        const auto* values = findActorValues(actor.getCellRef().getFormKey());
        if (!values)
            throw std::invalid_argument("native nonplayer actor values have not been initialized");
        validateNonPlayerIdentity(actor, *values);
        return *values;
    }

    void OblivionCombatService::publishNonPlayerValues(const MWWorld::Ptr& actor, ESM4::RuntimeActorValues values)
    {
        values.validate();
        validateNonPlayerIdentity(actor, values);
        const auto projection = actorProjection(values);
        std::optional<OblivionActorProjection> prepared;
        std::array<float, 21> preparedSkills{};
        std::array<float, 21>* creatureSkills = nullptr;
        if (actor.getType() == ESM::REC_NPC_4)
            prepared.emplace(actor.getClass().getNpcStats(actor), projection);
        else
        {
            for (std::size_t i = 0; i < preparedSkills.size(); ++i)
                preparedSkills[i] = nonPlayerFloat(values,
                    nonPlayerValueIndex(actor.getType() == ESM::REC_CREA4, static_cast<std::uint8_t>(12 + i)));
            prepared.emplace(actor.getClass().getCreatureStats(actor), projection);
            auto& data = actor.getRefData().getCustomData()->asESM4CreatureCustomData();
            if (!data.mNativeSkills)
                throw std::logic_error("native creature lacks its skill projection");
            creatureSkills = &*data.mNativeSkills;
        }
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
        prepared->commit();
        if (creatureSkills)
            *creatureSkills = preparedSkills;
    }

    void OblivionCombatService::changeNonPlayerValue(const MWWorld::Ptr& actor, std::uint8_t value,
        ESM4::ActorValueModifier modifier, float delta)
    {
        validateNonPlayerQuery(value);
        auto candidate = nonPlayerValues(actor);
        value = nonPlayerValueIndex(actor.getType() == ESM::REC_CREA4, value);
        candidate.mValues[value] = ESM4::changeActorValueModifier(
            candidate.mValues[value], candidate.mOwner, modifier, delta);
        publishNonPlayerValues(actor, std::move(candidate));
    }

    float OblivionCombatService::getNonPlayerValue(const MWWorld::Ptr& actor, std::uint8_t value) const
    {
        const auto& values = nonPlayerValues(actor);
        return nonPlayerFloat(values, nonPlayerValueIndex(actor.getType() == ESM::REC_CREA4, value));
    }

    std::int32_t OblivionCombatService::getNonPlayerIntegerValue(const MWWorld::Ptr& actor, std::uint8_t value) const
    {
        const auto& values = nonPlayerValues(actor);
        return nonPlayerInteger(values, nonPlayerValueIndex(actor.getType() == ESM::REC_CREA4, value));
    }

    const ESM4::RuntimeActorValues& OblivionCombatService::nonPlayerValues(const ESM::FormKey& actor) const
    {
        const auto* values = findActorValues(actor);
        if (!values || values->mOwner != ESM4::ActorValueOwner::NonPlayer)
            throw std::invalid_argument("native nonplayer values have not been initialized");
        return *values;
    }

    float OblivionCombatService::getNonPlayerValue(
        const ESM::FormKey& actor, std::uint8_t value, const MWWorld::ESMStore& store) const
    {
        const auto& values = nonPlayerValues(actor);
        return nonPlayerFloat(values, nonPlayerValueIndex(nonPlayerContentIsCreature(values, store), value));
    }

    std::int32_t OblivionCombatService::getNonPlayerIntegerValue(
        const ESM::FormKey& actor, std::uint8_t value, const MWWorld::ESMStore& store) const
    {
        const auto& values = nonPlayerValues(actor);
        return nonPlayerInteger(values, nonPlayerValueIndex(nonPlayerContentIsCreature(values, store), value));
    }

    std::int32_t OblivionCombatService::getNonPlayerBaseValue(
        const ESM::FormKey& actor, std::uint8_t value, const MWWorld::ESMStore& store) const
    {
        if (value >= 72)
            throw std::invalid_argument("invalid native base actor-value query");
        const auto& values = nonPlayerValues(actor);
        nonPlayerContentIsCreature(values, store);
        return ESM4::combatBaseValue(values.mValues[value].mBase);
    }

    std::int32_t OblivionCombatService::getPlayerBaseValue(std::uint8_t value) const
    {
        if (value >= 72)
            throw std::invalid_argument("invalid native base actor-value query");
        return ESM4::combatBaseValue(playerValues().mValues[value].mBase);
    }

    const ESM4::RuntimeActorValues& OblivionCombatService::playerValues() const
    {
        const auto* values = findActorValues(ESM::FormKey::dynamic("player", 1));
        if (!values)
            throw std::invalid_argument("native player values have not been initialized");
        validatePlayerIdentity(*values);
        return *values;
    }

    void OblivionCombatService::publishPlayerValues(MWWorld::Player& player, ESM4::RuntimeActorValues values,
        const ESM4::PlayerDynamicBaseSettings& settings)
    {
        values.validate();
        validatePlayerIdentity(values);
        std::array<std::int32_t, 8> attributes;
        for (std::size_t i = 0; i < attributes.size(); ++i)
            attributes[i] = ESM4::composeIntegerActorValue(ESM4::combatBaseValue(values.mValues[i].mBase),
                values.mValues[i].mModifiers, values.mOwner, values.mProcess);
        const auto magickaMultiplier = ESM4::composeActorValue(values.mValues[40], values.mOwner, values.mProcess);
        for (std::size_t i = 0; i < values.mPlayerFormValues->size(); ++i)
            values.mValues[8 + i].mBase = ESM4::calculatePlayerDynamicBaseValue(
                {static_cast<ESM4::DynamicActorValue>(8 + i), (*values.mPlayerFormValues)[i],
                    attributes, magickaMultiplier}, settings);
        values.validate();
        const auto ptr = player.getPlayer();
        OblivionActorProjection prepared(ptr.getClass().getNpcStats(ptr), actorProjection(values));
        const auto found = mActorValues.find(values.mActor);
        if (found == mActorValues.end())
        {
            const auto key = values.mActor;
            mActorValues.emplace(key, std::move(values));
        }
        else
            std::swap(found->second, values);
        prepared.commit();
    }

    void OblivionCombatService::changePlayerValue(MWWorld::Player& player, std::uint8_t value,
        ESM4::ActorValueModifier modifier, float delta, const ESM4::PlayerDynamicBaseSettings& settings)
    {
        validatePlayerQuery(value);
        auto candidate = playerValues();
        candidate.mValues[value] = ESM4::changeActorValueModifier(
            candidate.mValues[value], candidate.mOwner, modifier, delta);
        publishPlayerValues(player, std::move(candidate), settings);
    }

    void OblivionCombatService::regenerateNonPlayerFatigue(const MWWorld::Ptr& actor, float duration,
        const ESM4::FatigueRegenerationSettings& settings)
    {
        const auto& values = nonPlayerValues(actor);
        if (actor.getClass().getCreatureStats(actor).isDead())
            return;
        const float delta = fatigueRestoration(values, duration, settings);
        if (delta > 0)
            changeNonPlayerValue(actor, 10, ESM4::ActorValueModifier::Damage, delta);
    }

    void OblivionCombatService::regeneratePlayerFatigue(MWWorld::Player& player, float duration,
        const ESM4::FatigueRegenerationSettings& settings, const ESM4::PlayerDynamicBaseSettings& baseSettings)
    {
        const auto& values = playerValues();
        const auto ptr = player.getPlayer();
        if (ptr.getClass().getCreatureStats(ptr).isDead())
            return;
        const float delta = fatigueRestoration(values, duration, settings);
        if (delta > 0)
            changePlayerValue(player, 10, ESM4::ActorValueModifier::Damage, delta, baseSettings);
    }

    float OblivionCombatService::getPlayerValue(std::uint8_t value) const
    {
        validatePlayerQuery(value);
        const auto& values = playerValues();
        return ESM4::composeActorValue(values.mValues[value], values.mOwner, values.mProcess);
    }

    std::int32_t OblivionCombatService::getPlayerIntegerValue(std::uint8_t value) const
    {
        validatePlayerQuery(value);
        const auto& values = playerValues();
        const auto& state = values.mValues[value];
        return ESM4::composeIntegerActorValue(
            ESM4::combatBaseValue(state.mBase), state.mModifiers, values.mOwner, values.mProcess);
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
        {
            if (state.mVersion < 10 && actor.mPlayerFormValues)
                throw std::invalid_argument("native player form values require an Oblivion v10+ save");
            actors.push_back(actor);
        }
        auto actions = mActions.capture();
        state.mNativeActorValues.swap(actors);
        state.mPhysicalActions = std::move(actions);
    }

    void OblivionCombatService::restore(const ESM4::RuntimeState& state, const MWWorld::ESMStore& store)
    {
        state.validate();
        for (const auto& actor : state.mNativeActorValues)
            if (actor.mOwner == ESM4::ActorValueOwner::NonPlayer)
                nonPlayerContentIsCreature(actor, store);
        restore(state);
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
