#include "oblivioncombat.hpp"

#include <components/esm4/runtimestate.hpp>

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <optional>
#include <list>
#include <set>

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

        bool nativeBaseIsCreature(const ESM::FormKey& base, const MWWorld::ESMStore& store)
        {
            const auto* npc = store.search<ESM4::Npc>(base);
            const auto* creature = store.search<ESM4::Creature>(base);
            if ((!npc && !creature) || (npc && creature)
                || (npc && (!npc->mIsTES4 || npc->mFormKey != base))
                || (creature && (!creature->mAttackReach || creature->mFormKey != base)))
                throw std::invalid_argument("missing, ambiguous or unsupported native actor base: " + base.serialize());
            return creature != nullptr;
        }

        bool nonPlayerContentIsCreature(const ESM4::RuntimeActorValues& values, const MWWorld::ESMStore& store)
        {
            if (values.mOwner != ESM4::ActorValueOwner::NonPlayer)
                throw std::invalid_argument("native nonplayer content lookup has player ownership");
            const bool creature = nativeBaseIsCreature(values.mBase, store);
            if (values.mActor.isContent())
            {
                const auto* characterRef = store.search<ESM4::ActorCharacter>(values.mActor);
                const auto* creatureRef = store.search<ESM4::ActorCreature>(values.mActor);
                const ESM4::ActorCharacter* reference = creature ? creatureRef : characterRef;
                if (!reference || (characterRef && creatureRef) || reference->mFormKey != values.mActor
                    || reference->mBaseKey != values.mBase)
                    throw std::invalid_argument("missing or mismatched native actor reference: " + values.mActor.serialize());
            }
            return creature;
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

        std::optional<std::int32_t> integerBaseOverride(const ESM4::RuntimeActorBaseOverride* base, std::uint8_t value)
        {
            if (base)
                for (const auto& entry : base->mValues)
                    if (entry.mActorValue == value)
                        return ESM4::actorBaseValueInteger(entry);
            return std::nullopt;
        }

        float nonPlayerFloat(const ESM4::RuntimeActorValues& values, std::uint8_t value,
            const ESM4::RuntimeActorBaseOverride* base = nullptr)
        {
            validateNonPlayerQuery(value);
            const auto integerBase = integerBaseOverride(base, value);
            const auto current = integerBase
                ? ESM4::composeNonPlayerActorValue(*integerBase, values.mValues[value].mModifiers, values.mProcess)
                : ESM4::composeActorValue(values.mValues[value], values.mOwner, values.mProcess);
            if (value != 9)
                return current;
            const auto multiplier = ESM4::composeActorValue(values.mValues[40], values.mOwner, values.mProcess);
            return ESM4::scaleNpcMagicka(current, multiplier);
        }

        std::int32_t nonPlayerInteger(const ESM4::RuntimeActorValues& values, std::uint8_t value,
            const ESM4::RuntimeActorBaseOverride* base)
        {
            validateNonPlayerQuery(value);
            const auto& state = values.mValues[value];
            const auto override = integerBaseOverride(base, value);
            const auto integerBase = override ? *override : ESM4::combatBaseValue(state.mBase);
            const auto current = ESM4::composeIntegerActorValue(
                integerBase, state.mModifiers, values.mOwner, values.mProcess);
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

        bool restoreResources(ESM4::RuntimeActorValues& values, const OblivionRestorationUpdate& input,
            const OblivionRestorationSettings& settings, const ESM4::RuntimeActorBaseOverride* base = nullptr)
        {
            const auto current = [&](std::uint8_t av) {
                return values.mOwner == ESM4::ActorValueOwner::Player
                    ? ESM4::composeActorValue(values.mValues[av], values.mOwner, values.mProcess)
                    : nonPlayerFloat(values, av, base);
            };
            const auto integer = [&](std::uint8_t av) {
                const auto& value = values.mValues[av];
                return ESM4::composeIntegerActorValue(ESM4::combatBaseValue(value.mBase), value.mModifiers,
                    values.mOwner, values.mProcess);
            };
            const auto maximum = [&](std::uint8_t av) {
                return values.mOwner == ESM4::ActorValueOwner::Player
                    || values.mProcess == ESM4::ActorValueProcess::Active
                    ? values.mValues[av].mModifiers[0].value_or(0.f) : 0.f;
            };
            bool changed = false;
            const auto restore = [&](std::uint8_t av, float delta) {
                if (delta > 0)
                {
                    values.mValues[av] = ESM4::changeActorValueModifier(
                        values.mValues[av], values.mOwner, ESM4::ActorValueModifier::Damage, delta);
                    changed = true;
                }
            };
            if (input.mRestoreHealth)
                restore(8, ESM4::healthRestoration(current(8), ESM4::combatBaseValue(values.mValues[8].mBase), maximum(8)));
            restore(9, ESM4::magickaRegeneration({current(9), ESM4::combatBaseValue(values.mValues[9].mBase),
                maximum(9), integer(2), integer(57), input.mDuration, input.mHasActiveMagicItem, true}, settings.mMagicka));
            restore(10, fatigueRestoration(values, input.mDuration, settings.mFatigue));
            return changed;
        }

        ESM4::MovementFatigueInput movementFatigueInput(const ESM4::RuntimeActorValues& values,
            std::int32_t encumbrance, std::uint8_t skill)
        {
            return {ESM4::composeActorValue(values.mValues[10], values.mOwner, values.mProcess),
                ESM4::composeActorValue(values.mValues[0], values.mOwner, values.mProcess), encumbrance,
                ESM4::combatBaseValue(values.mValues[skill].mBase)};
        }

        void changeFatigueDamage(ESM4::RuntimeActorValues& values, float delta)
        {
            values.mValues[10] = ESM4::changeActorValueModifier(
                values.mValues[10], values.mOwner, ESM4::ActorValueModifier::Damage, delta);
        }

        bool updateFatigue(ESM4::RuntimeActorValues& values, const OblivionFatigueUpdate& input,
            const OblivionFatigueSettings& settings)
        {
            bool changed = false;
            if (input.mRunning && input.mCanSpend)
            {
                const float debit = ESM4::runningFatigueDebit(movementFatigueInput(values, input.mEncumbrance, 13),
                    input.mDuration, settings.mMovement, settings.mMastery);
                if (debit > 0)
                {
                    changeFatigueDamage(values, -debit);
                    changed = true;
                }
            }
            const float restoration = fatigueRestoration(values, input.mDuration, settings.mRegeneration);
            if (restoration > 0)
            {
                changeFatigueDamage(values, restoration);
                changed = true;
            }
            return changed;
        }

        bool spendJumpFatigue(ESM4::RuntimeActorValues& values, std::int32_t encumbrance, bool canSpend,
            const OblivionFatigueSettings& settings)
        {
            if (!canSpend)
                return false;
            const float debit = ESM4::jumpingFatigueDebit(movementFatigueInput(values, encumbrance, 26),
                settings.mMovement, settings.mMastery);
            if (debit <= 0)
                return false;
            changeFatigueDamage(values, -debit);
            return true;
        }

        OblivionActorProjectionInput actorProjection(const ESM4::RuntimeActorValues& values,
            const ESM4::RuntimeActorBaseOverride* base = nullptr)
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
                        : nonPlayerFloat(values, static_cast<std::uint8_t>(8 + i), base)};
            }
            return input;
        }

        void applyActorBase(ESM4::RuntimeActorValues& values, const ESM4::RuntimeActorBaseOverride* base)
        {
            if (!base)
                return;
            if (base->mBase != values.mBase
                || (values.mOwner == ESM4::ActorValueOwner::Player && base->mKind != ESM4::ActorBaseKind::Npc))
                throw std::invalid_argument("native actor base override identity mismatch");
            for (const auto& entry : base->mValues)
            {
                const auto av = entry.mActorValue;
                if (values.mOwner == ESM4::ActorValueOwner::Player && av >= 8 && av <= 10)
                {
                    validatePlayerIdentity(values);
                    (*values.mPlayerFormValues)[av - 8] = std::get<std::int32_t>(entry.mValue);
                    continue; // Derived player bases require winning settings.
                }
                const float resolved = static_cast<float>(ESM4::actorBaseValueInteger(entry));
                values.mValues[av].mBase = resolved;
                if (base->mKind == ESM4::ActorBaseKind::Creature && av >= 12 && av <= 26)
                    for (std::size_t i = av + 1; i < av + 7u; ++i)
                        values.mValues[i].mBase = resolved;
            }
        }

        void setActorBaseEntry(ESM4::RuntimeActorBaseOverride& base, const ESM4::ActorBaseValueSet& entry)
        {
            const auto found = std::find_if(base.mValues.begin(), base.mValues.end(),
                [&](const auto& item) { return item.mActorValue == entry.mActorValue; });
            if (found == base.mValues.end())
                base.mValues.push_back(entry);
            else
                *found = entry;
            base.validate();
        }

    }

    class OblivionCombatService::PreparedNonPlayerView
    {
        std::optional<OblivionActorProjection> mProjection;
        std::array<float, 21> mSkills{};
        std::array<float, 21>* mSkillTarget = nullptr;

    public:
        PreparedNonPlayerView(const MWWorld::Ptr& actor, const ESM4::RuntimeActorValues& values,
            const ESM4::RuntimeActorBaseOverride* base)
        {
            validateNonPlayerIdentity(actor, values);
            const auto projection = actorProjection(values, base);
            if (actor.getType() == ESM::REC_NPC_4)
                mProjection.emplace(actor.getClass().getNpcStats(actor), projection);
            else
            {
                for (std::size_t i = 0; i < mSkills.size(); ++i)
                    mSkills[i] = nonPlayerFloat(values, nonPlayerValueIndex(true, static_cast<std::uint8_t>(12 + i)));
                mProjection.emplace(actor.getClass().getCreatureStats(actor), projection);
                auto& data = actor.getRefData().getCustomData()->asESM4CreatureCustomData();
                if (!data.mNativeSkills)
                    throw std::logic_error("native creature lacks its skill projection");
                mSkillTarget = &*data.mNativeSkills;
            }
        }

        void commit() noexcept
        {
            mProjection->commit();
            if (mSkillTarget)
                *mSkillTarget = mSkills;
        }
    };

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
        mActorBases.clear();
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
        const auto* base = findActorBase(values.mBase);
        if (base && (base->mKind == ESM4::ActorBaseKind::Creature) != (actor.getType() == ESM::REC_CREA4))
            throw std::invalid_argument("native actor base override kind mismatch");
        applyActorBase(values, base);
        values.validate();
        PreparedNonPlayerView prepared(actor, values, base);
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

    const ESM4::RuntimeActorBaseOverride* OblivionCombatService::findActorBase(const ESM::FormKey& base) const
    {
        const auto found = mActorBases.find(base);
        return found == mActorBases.end() ? nullptr : &found->second;
    }

    void OblivionCombatService::setNonPlayerBaseValue(const MWWorld::Ptr& actor, std::uint8_t value,
        std::int32_t requested, std::span<const MWWorld::Ptr> residents)
    {
        validateNonPlayerQuery(value);
        const auto& target = nonPlayerValues(actor);
        const auto kind = actor.getType() == ESM::REC_CREA4 ? ESM4::ActorBaseKind::Creature : ESM4::ActorBaseKind::Npc;
        const auto entry = ESM4::prepareActorBaseValueSet(kind, value, requested);
        if (!entry)
            return;
        auto bases = mActorBases;
        auto [baseIt, inserted] = bases.try_emplace(target.mBase,
            ESM4::RuntimeActorBaseOverride{target.mBase, kind, {}});
        auto& base = baseIt->second;
        if (base.mKind != kind)
            throw std::invalid_argument("native actor base override kind mismatch");
        setActorBaseEntry(base, *entry);
        auto actors = mActorValues;
        for (auto& [key, values] : actors)
            if (values.mBase == target.mBase)
            {
                if (values.mOwner != ESM4::ActorValueOwner::NonPlayer)
                    throw std::invalid_argument("native shared base has mixed player ownership");
                applyActorBase(values, &base);
                values.validate();
                // Validate complete output even for an unloaded reference.
                actorProjection(values, &base);
            }
        std::set<ESM::FormKey> identities;
        std::list<PreparedNonPlayerView> prepared;
        for (const auto& ptr : residents)
        {
            const auto& old = nonPlayerValues(ptr);
            if (old.mBase != target.mBase || !identities.insert(old.mActor).second
                || (ptr.getType() == ESM::REC_CREA4) != (kind == ESM4::ActorBaseKind::Creature))
                throw std::invalid_argument("invalid or duplicate native shared-base resident");
            prepared.emplace_back(ptr, actors.at(old.mActor), &base);
        }
        if (!identities.contains(target.mActor))
            throw std::invalid_argument("native shared-base transaction omits its target");
        mActorBases.swap(bases);
        mActorValues.swap(actors);
        for (auto& view : prepared)
            view.commit();
    }

    void OblivionCombatService::setPlayerBaseValue(MWWorld::Player& player, std::uint8_t value,
        std::int32_t requested, const ESM4::PlayerDynamicBaseSettings& settings)
    {
        validatePlayerQuery(value);
        auto candidate = playerValues();
        const auto entry = ESM4::prepareActorBaseValueSet(ESM4::ActorBaseKind::Npc, value, requested);
        if (!entry)
            return;
        auto bases = mActorBases;
        auto [baseIt, inserted] = bases.try_emplace(candidate.mBase,
            ESM4::RuntimeActorBaseOverride{candidate.mBase, ESM4::ActorBaseKind::Npc, {}});
        if (baseIt->second.mKind != ESM4::ActorBaseKind::Npc)
            throw std::invalid_argument("native player base override kind mismatch");
        setActorBaseEntry(baseIt->second, *entry);
        applyActorBase(candidate, &baseIt->second);
        // Publish on an isolated candidate authority so all preparation can
        // fail before the live base map, actor map or shared views change.
        OblivionCombatService prepared;
        prepared.mActorValues = mActorValues;
        prepared.mActorBases = std::move(bases);
        prepared.publishPlayerValues(player, std::move(candidate), settings);
        mActorValues.swap(prepared.mActorValues);
        mActorBases.swap(prepared.mActorBases);
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

    OblivionActorValueCommandResult OblivionCombatService::executeNonPlayerValueCommand(
        const MWWorld::Ptr& actor, std::uint8_t value, ESM4::ActorValueCommand command,
        ESM4::ActorValueCommandSource source, std::int32_t requested,
        const ESM4::ActorValueCommandPolicy& policy, std::span<const MWWorld::Ptr> residents)
    {
        validateNonPlayerQuery(value);
        nonPlayerValues(actor);
        if (source != ESM4::ActorValueCommandSource::Script && source != ESM4::ActorValueCommandSource::Console)
            throw std::invalid_argument("invalid native actor-value command source");
        if (command == ESM4::ActorValueCommand::Set)
        {
            setNonPlayerBaseValue(actor, value, requested, residents);
            return {true, std::nullopt};
        }
        const auto change = ESM4::prepareActorValueModifierCommand(ESM4::ActorValueOwner::NonPlayer,
            value, command, source, requested,
            command == ESM4::ActorValueCommand::Force ? getNonPlayerValue(actor, value) : 0.f, policy);
        if (!change)
            return {false, std::nullopt};
        changeNonPlayerValue(actor, value, change->mModifier, change->mDelta);
        return {true, change->mHealthReaction ? std::optional(change->mDelta) : std::nullopt};
    }

    OblivionActorValueCommandResult OblivionCombatService::executePlayerValueCommand(
        MWWorld::Player& player, std::uint8_t value, ESM4::ActorValueCommand command,
        ESM4::ActorValueCommandSource source, std::int32_t requested,
        const ESM4::ActorValueCommandPolicy& policy, const ESM4::PlayerDynamicBaseSettings& settings)
    {
        validatePlayerQuery(value);
        playerValues();
        if (source != ESM4::ActorValueCommandSource::Script && source != ESM4::ActorValueCommandSource::Console)
            throw std::invalid_argument("invalid native actor-value command source");
        if (command == ESM4::ActorValueCommand::Set)
        {
            setPlayerBaseValue(player, value, requested, settings);
            return {true, std::nullopt};
        }
        const auto change = ESM4::prepareActorValueModifierCommand(ESM4::ActorValueOwner::Player,
            value, command, source, requested,
            command == ESM4::ActorValueCommand::Force ? getPlayerValue(value) : 0.f, policy);
        if (!change)
            return {false, std::nullopt};
        changePlayerValue(player, value, change->mModifier, change->mDelta, settings);
        return {true, change->mHealthReaction ? std::optional(change->mDelta) : std::nullopt};
    }

    float OblivionCombatService::getNonPlayerValue(const MWWorld::Ptr& actor, std::uint8_t value) const
    {
        const auto& values = nonPlayerValues(actor);
        return nonPlayerFloat(values, nonPlayerValueIndex(actor.getType() == ESM::REC_CREA4, value),
            findActorBase(values.mBase));
    }

    std::int32_t OblivionCombatService::getNonPlayerIntegerValue(const MWWorld::Ptr& actor, std::uint8_t value) const
    {
        const auto& values = nonPlayerValues(actor);
        return nonPlayerInteger(values, nonPlayerValueIndex(actor.getType() == ESM::REC_CREA4, value),
            findActorBase(values.mBase));
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
        return nonPlayerFloat(values, nonPlayerValueIndex(nonPlayerContentIsCreature(values, store), value),
            findActorBase(values.mBase));
    }

    std::int32_t OblivionCombatService::getNonPlayerIntegerValue(
        const ESM::FormKey& actor, std::uint8_t value, const MWWorld::ESMStore& store) const
    {
        const auto& values = nonPlayerValues(actor);
        return nonPlayerInteger(values, nonPlayerValueIndex(nonPlayerContentIsCreature(values, store), value),
            findActorBase(values.mBase));
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

    double OblivionCombatService::getScriptActorValue(const ESM::FormKey& actor, std::uint8_t value,
        bool base, bool disabled, const MWWorld::ESMStore& store) const
    {
        if (value >= 72)
            throw std::invalid_argument("invalid native script actor-value query");
        const bool player = actor == ESM::FormKey::dynamic("player", 1);
        if (base)
            return player ? getPlayerBaseValue(value) : getNonPlayerBaseValue(actor, value, store);
        if (!disabled)
            return player ? getPlayerValue(value) : getNonPlayerValue(actor, value, store);
        const auto& values = player ? playerValues() : nonPlayerValues(actor);
        if (!player)
            nonPlayerContentIsCreature(values, store);
        if (value >= 37 && value <= 39)
            return 0; // These are reference queries, not base-form fields.
        if (player && value >= 8 && value <= 11)
            return (*values.mPlayerFormValues)[value - 8];
        if (const auto raw = integerBaseOverride(findActorBase(values.mBase), value))
            return *raw;
        return ESM4::actorBaseValueInteger({value, values.mValues[value].mBase});
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
        applyActorBase(values, findActorBase(values.mBase));
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

    void OblivionCombatService::restoreNonPlayerResources(const MWWorld::Ptr& actor,
        const OblivionRestorationUpdate& input, const OblivionRestorationSettings& settings)
    {
        auto candidate = nonPlayerValues(actor);
        if (!actor.getClass().getCreatureStats(actor).isDead()
            && restoreResources(candidate, input, settings, findActorBase(candidate.mBase)))
            publishNonPlayerValues(actor, std::move(candidate));
    }

    void OblivionCombatService::restorePlayerResources(MWWorld::Player& player,
        const OblivionRestorationUpdate& input, const OblivionRestorationSettings& settings)
    {
        auto candidate = playerValues();
        const auto ptr = player.getPlayer();
        if (!ptr.getClass().getCreatureStats(ptr).isDead() && restoreResources(candidate, input, settings))
            publishPlayerValues(player, std::move(candidate), settings.mPlayerBase);
    }

    void OblivionCombatService::updateNonPlayerFatigue(const MWWorld::Ptr& actor,
        const OblivionFatigueUpdate& input, const OblivionFatigueSettings& settings)
    {
        auto candidate = nonPlayerValues(actor);
        if (!actor.getClass().getCreatureStats(actor).isDead() && updateFatigue(candidate, input, settings))
            publishNonPlayerValues(actor, std::move(candidate));
    }

    void OblivionCombatService::updatePlayerFatigue(MWWorld::Player& player,
        const OblivionFatigueUpdate& input, const OblivionFatigueSettings& settings)
    {
        auto candidate = playerValues();
        const auto ptr = player.getPlayer();
        if (!ptr.getClass().getCreatureStats(ptr).isDead() && updateFatigue(candidate, input, settings))
            publishPlayerValues(player, std::move(candidate), settings.mPlayerBase);
    }

    void OblivionCombatService::spendPlayerJumpFatigue(MWWorld::Player& player,
        std::int32_t encumbrance, bool canSpend, const OblivionFatigueSettings& settings)
    {
        auto candidate = playerValues();
        const auto ptr = player.getPlayer();
        if (!ptr.getClass().getCreatureStats(ptr).isDead() && spendJumpFatigue(candidate, encumbrance, canSpend, settings))
            publishPlayerValues(player, std::move(candidate), settings.mPlayerBase);
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
        if (state.mVersion < 11 && !mActorBases.empty())
            throw std::invalid_argument("native actor base overrides require an Oblivion v11+ save");
        std::vector<ESM4::RuntimeActorBaseOverride> bases;
        bases.reserve(mActorBases.size());
        for (const auto& [key, base] : mActorBases)
            bases.push_back(base);
        std::vector<ESM4::RuntimeActorValues> actors;
        actors.reserve(mActorValues.size());
        for (const auto& [key, actor] : mActorValues)
        {
            if (state.mVersion < 10 && actor.mPlayerFormValues)
                throw std::invalid_argument("native player form values require an Oblivion v10+ save");
            actors.push_back(actor);
        }
        auto actions = mActions.capture();
        state.mNativeActorBases.swap(bases);
        state.mNativeActorValues.swap(actors);
        state.mPhysicalActions = std::move(actions);
    }

    void OblivionCombatService::restore(const ESM4::RuntimeState& state, const MWWorld::ESMStore& store)
    {
        state.validate();
        for (const auto& actor : state.mNativeActorValues)
            if (actor.mOwner == ESM4::ActorValueOwner::NonPlayer)
                nonPlayerContentIsCreature(actor, store);
        for (const auto& base : state.mNativeActorBases)
        {
            const bool creature = base.mBase == ESM::FormKey::dynamic("player-base", 1)
                ? false : nativeBaseIsCreature(base.mBase, store);
            if (creature != (base.mKind == ESM4::ActorBaseKind::Creature))
                throw std::invalid_argument("native actor base override kind mismatch: " + base.mBase.serialize());
        }
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
        std::map<ESM::FormKey, ESM4::RuntimeActorBaseOverride> bases;
        for (const auto& base : state.mNativeActorBases)
            bases.emplace(base.mBase, base);
        for (const auto& [key, actor] : actors)
        {
            const auto base = bases.find(actor.mBase);
            if (base == bases.end())
                continue;
            auto resolved = actor;
            applyActorBase(resolved, &base->second);
            if (resolved != actor)
                throw std::invalid_argument("native actor snapshot disagrees with shared base override: " + key.serialize());
        }
        mActions = std::move(actions);
        mActorValues.swap(actors);
        mActorBases.swap(bases);
    }
}
