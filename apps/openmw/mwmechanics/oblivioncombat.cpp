#include "oblivioncombat.hpp"

#include <components/esm4/runtimestate.hpp>
#include <components/esm4/actorclock.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <optional>
#include <list>
#include <limits>
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

        template <class Update>
        void updateWithActorClock(std::map<ESM::FormKey, float>& times, const ESM::FormKey& actor,
            float time, Update&& update)
        {
            const auto found = times.find(actor);
            const float elapsed = ESM4::actorUpdateDuration(time, found == times.end() ? -1.f : found->second);
            // Allocate a missing entry before touching resource authority. The
            // node transfer below allocates nothing; FormKey comparison only
            // compares its existing kind/string/integer members.
            std::map<ESM::FormKey, float> prepared;
            if (found == times.end())
                prepared.emplace(actor, time);
            update(elapsed);
            if (found == times.end())
                times.insert(prepared.extract(prepared.begin()));
            else
                found->second = time;
        }

        float statModifierDelta(const ESM4::RuntimeActorValues& values, std::uint8_t value,
            ESM4::ActorValueModifier modifier, float requested)
        {
            if (!(value < 8 || (value >= 12 && value <= 32))
                || (modifier != ESM4::ActorValueModifier::Maximum && modifier != ESM4::ActorValueModifier::Damage)
                || !std::isfinite(requested))
                throw std::invalid_argument("native stat modifier request requires an attribute or skill channel");
            const float old = values.mValues[value].mModifiers[static_cast<unsigned>(modifier)].value_or(0.f);
            const float delta = static_cast<float>(double(requested) - old);
            if (!std::isfinite(delta))
                throw std::invalid_argument("native stat modifier request delta overflow");
            return delta;
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

        float magickaRestoration(const ESM4::RuntimeActorValues& values, float duration,
            bool hasActiveMagicItem, const ESM4::MagickaRegenerationSettings& settings,
            const ESM4::RuntimeActorBaseOverride* base = nullptr)
        {
            const auto& magicka = values.mValues[9];
            const auto integer = [&](std::uint8_t av) {
                const auto& value = values.mValues[av];
                return ESM4::composeIntegerActorValue(ESM4::combatBaseValue(value.mBase), value.mModifiers,
                    values.mOwner, values.mProcess);
            };
            const float current = values.mOwner == ESM4::ActorValueOwner::Player
                ? ESM4::composeActorValue(magicka, values.mOwner, values.mProcess)
                : nonPlayerFloat(values, 9, base);
            const float maximumModifier = values.mOwner == ESM4::ActorValueOwner::Player
                || values.mProcess == ESM4::ActorValueProcess::Active
                ? magicka.mModifiers[0].value_or(0.f) : 0.f;
            return ESM4::magickaRegeneration({current, ESM4::combatBaseValue(magicka.mBase), maximumModifier,
                integer(2), integer(57), duration, hasActiveMagicItem, true}, settings);
        }

        bool restoreResources(ESM4::RuntimeActorValues& values, const OblivionRestorationUpdate& input,
            const OblivionRestorationSettings& settings, const ESM4::RuntimeActorBaseOverride* base = nullptr)
        {
            const auto current = [&](std::uint8_t av) {
                return values.mOwner == ESM4::ActorValueOwner::Player
                    ? ESM4::composeActorValue(values.mValues[av], values.mOwner, values.mProcess)
                    : nonPlayerFloat(values, av, base);
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
                        values.mValues[av], values.mOwner, av, ESM4::ActorValueModifier::Damage, delta);
                    changed = true;
                }
            };
            if (input.mRestoreHealth)
                restore(8, ESM4::healthRestoration(current(8), ESM4::combatBaseValue(values.mValues[8].mBase), maximum(8)));
            restore(9, magickaRestoration(values, input.mDuration, input.mHasActiveMagicItem, settings.mMagicka, base));
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
                values.mValues[10], values.mOwner, 10, ESM4::ActorValueModifier::Damage, delta);
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

        bool updateFrameResources(ESM4::RuntimeActorValues& values, const OblivionFatigueUpdate& input,
            bool hasActiveMagicItem, const OblivionFrameSettings& settings,
            const ESM4::RuntimeActorBaseOverride* base = nullptr)
        {
            const float magicka = magickaRestoration(values, input.mDuration, hasActiveMagicItem, settings.mMagicka, base);
            if (magicka > 0)
                values.mValues[9] = ESM4::changeActorValueModifier(
                    values.mValues[9], values.mOwner, 9, ESM4::ActorValueModifier::Damage, magicka);
            // Evaluate Fatigue even when Magicka changed; both remain staged on failure.
            return updateFatigue(values, input, settings.mFatigue) || magicka > 0;
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

        void prepareResurrectionReset(ESM4::RuntimeActorValues& values, ESM4::RuntimeActorLife& life)
        {
            for (std::uint8_t av = 0; av < values.mValues.size(); ++av)
                values.mValues[av] = ESM4::resetResurrectionModifiers(values.mValues[av], values.mOwner, av);
            values.mProcess = values.mOwner == ESM4::ActorValueOwner::Player
                ? ESM4::ActorValueProcess::Active : ESM4::ActorValueProcess::Low;
            life.mPhase = ESM4::ActorLifePhase::Alive;
            life.mRecoveryRemaining = 0;
            life.mKiller = {};
            values.validate();
            life.validate();
        }

        OblivionActorProjectionInput actorProjection(const ESM4::RuntimeActorValues& values,
            const ESM4::RuntimeActorBaseOverride* base = nullptr, const ESM4::RuntimeActorLife* life = nullptr)
        {
            OblivionActorProjectionInput input;
            if (life)
            {
                life->validate();
                if (life->mActor != values.mActor || life->mBase != values.mBase)
                    throw std::invalid_argument("native life projection identity mismatch");
                input.mLife = life->mPhase;
            }
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
            const ESM4::RuntimeActorBaseOverride* base, const ESM4::RuntimeActorLife* life = nullptr)
        {
            validateNonPlayerIdentity(actor, values);
            const auto projection = actorProjection(values, base, life);
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
        , mLife(input.mLife)
    {
        if (mLife && *mLife != ESM4::ActorLifePhase::Alive && *mLife != ESM4::ActorLifePhase::Dead
            && *mLife != ESM4::ActorLifePhase::EssentialUnconscious)
            throw std::invalid_argument("invalid native lifecycle projection");
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
        if (mLife)
        {
            const bool dead = *mLife == ESM4::ActorLifePhase::Dead;
            const bool essential = *mLife == ESM4::ActorLifePhase::EssentialUnconscious;
            if (mTarget.isDead() != dead)
                mTarget.mDeathAnimationFinished = false;
            if (dead || (mTarget.mNativeEssentialUnconscious && !essential))
            {
                mTarget.mKnockdown = false;
                mTarget.mKnockdownOneFrame = false;
                mTarget.mKnockdownOverOneFrame = false;
            }
            mTarget.mNativeDead = dead;
            mTarget.mDead = dead;
            mTarget.mNativeEssentialUnconscious = essential;
        }
        mCommitted = true;
        return true;
    }

    void OblivionCombatService::clear()
    {
        mActions = {};
        mActorValues.clear();
        mActorBases.clear();
        mActorLife.clear();
        mNextDeathEvent = 1;
        mPendingDeathEvents.clear();
        mDeathCounts.clear();
        mActorBreath.clear();
        mActorManagerTime = 0;
        mActorUpdateTimes.clear();
        mCombatOpponents.clear();
    }

    void OblivionCombatService::advanceFrameClock(float duration)
    {
        if (!std::isfinite(duration) || duration < 0.f)
            throw std::invalid_argument("native frame clock requires finite nonnegative elapsed time");
        mActorManagerTime = ESM4::advanceActorManagerTime(mActorManagerTime, duration);
    }

    float OblivionCombatService::elapsedSinceActorUpdate(const ESM::FormKey& actor, float time) const
    {
        if (!mActorValues.contains(actor))
            throw std::invalid_argument("native actor clock requires registered actor values");
        if (!std::isfinite(time) || time > 100000.f)
            throw std::invalid_argument("invalid native actor manager time");
        const auto found = mActorUpdateTimes.find(actor);
        return ESM4::actorUpdateDuration(time, found == mActorUpdateTimes.end() ? -1.f : found->second);
    }

    bool OblivionCombatService::engage(const ESM::FormKey& actor, const ESM::FormKey& opponent)
    {
        if (actor == opponent)
            throw std::invalid_argument("native actor cannot engage itself");
        for (const auto* key : {&actor, &opponent})
        {
            const auto* life = findActorLife(*key);
            const auto* values = findActorValues(*key);
            if (!life || !values || life->mBase != values->mBase || life->mPhase == ESM4::ActorLifePhase::Dead)
                throw std::invalid_argument("native combat engagement requires initialized nonterminal actors");
        }
        if (isInCombatWith(actor, opponent))
            return false;
        auto prepared = mCombatOpponents;
        prepared[actor].insert(opponent);
        prepared[opponent].insert(actor);
        mCombatOpponents.swap(prepared);
        return true;
    }

    bool OblivionCombatService::stopCombat(const ESM::FormKey& actor) noexcept
    {
        const auto found = mCombatOpponents.find(actor);
        if (found == mCombatOpponents.end())
            return false;
        for (const auto& opponent : found->second)
        {
            const auto other = mCombatOpponents.find(opponent);
            other->second.erase(actor);
            if (other->second.empty())
                mCombatOpponents.erase(other);
        }
        mCombatOpponents.erase(found);
        return true;
    }

    bool OblivionCombatService::isInCombat(const ESM::FormKey& actor) const
    {
        return mCombatOpponents.contains(actor);
    }

    bool OblivionCombatService::isInCombatWith(const ESM::FormKey& actor, const ESM::FormKey& opponent) const
    {
        const auto found = mCombatOpponents.find(actor);
        return found != mCombatOpponents.end() && found->second.contains(opponent);
    }

    std::vector<ESM::FormKey> OblivionCombatService::combatOpponents(const ESM::FormKey& actor) const
    {
        const auto found = mCombatOpponents.find(actor);
        if (found == mCombatOpponents.end())
            return {};
        return {found->second.begin(), found->second.end()};
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
        PreparedNonPlayerView prepared(actor, values, base, findActorLife(values.mActor));
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
        if (!ESM4::prepareActorBaseValueSet(kind, value, requested))
            return;
        if (std::none_of(residents.begin(), residents.end(), [&](const auto& ptr) { return ptr == actor; }))
            throw std::invalid_argument("native shared-base transaction omits its target");
        setNonPlayerBaseValue(target, kind, value, requested, residents);
    }

    void OblivionCombatService::setNonPlayerBaseValue(const ESM4::RuntimeActorValues& target,
        ESM4::ActorBaseKind kind, std::uint8_t value, std::int32_t requested,
        std::span<const MWWorld::Ptr> residents)
    {
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
            prepared.emplace_back(ptr, actors.at(old.mActor), &base, findActorLife(old.mActor));
        }
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
        prepared.mActorLife = mActorLife;
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
            candidate.mValues[value], candidate.mOwner, value, modifier, delta);
        publishNonPlayerValues(actor, std::move(candidate));
    }

    void OblivionCombatService::requestNonPlayerStatModifier(const MWWorld::Ptr& actor, std::uint8_t value,
        ESM4::ActorValueModifier modifier, float requested)
    {
        const auto& values = nonPlayerValues(actor);
        // Lua exposes skills only on NPCs. Do not turn one creature group
        // request into a different visible skill through runtime aliases.
        if (value >= 12 && actor.getType() != ESM::REC_NPC_4)
            throw std::invalid_argument("native Lua skill modifier requires an NPC");
        changeNonPlayerValue(actor, value, modifier, statModifierDelta(values, value, modifier, requested));
    }

    void OblivionCombatService::requestPlayerStatModifier(MWWorld::Player& player, std::uint8_t value,
        ESM4::ActorValueModifier modifier, float requested, const ESM4::PlayerDynamicBaseSettings& settings)
    {
        changePlayerValue(player, value, modifier, statModifierDelta(playerValues(), value, modifier, requested), settings);
    }

    bool OblivionCombatService::changeNonPlayerHealth(const MWWorld::Ptr& actor, float delta,
        const ESM::FormKey& source, bool essential, const ESM4::EssentialRecoverySettings& settings)
    {
        auto candidate = nonPlayerValues(actor);
        candidate.mValues[8] = ESM4::changeActorValueModifier(
            candidate.mValues[8], candidate.mOwner, 8, ESM4::ActorValueModifier::Damage, delta);
        if (delta < 0.f)
            publishHealthChange(actor, std::move(candidate), essential, settings, false, source);
        else
            publishNonPlayerValues(actor, std::move(candidate));
        return true;
    }

    bool OblivionCombatService::changePlayerHealth(MWWorld::Player& player, float delta,
        const ESM::FormKey& source, bool essential, const ESM4::EssentialRecoverySettings& settings,
        const ESM4::PlayerDynamicBaseSettings& baseSettings, bool godMode)
    {
        auto candidate = playerValues();
        if (!std::isfinite(delta))
            throw std::invalid_argument("native Health delta must be finite");
        if (godMode && delta < 0.f)
            return false;
        candidate.mValues[8] = ESM4::changeActorValueModifier(
            candidate.mValues[8], candidate.mOwner, 8, ESM4::ActorValueModifier::Damage, delta);
        if (delta < 0.f)
        {
            preparePlayerValues(candidate, baseSettings);
            publishHealthChange(player.getPlayer(), std::move(candidate), essential, settings, godMode, source);
        }
        else
            publishPlayerValues(player, std::move(candidate), baseSettings);
        return true;
    }

    std::optional<OblivionBreathUpdateResult> OblivionCombatService::prepareBreathUpdate(
        const ESM4::RuntimeActorValues& values, float duration, bool needsAir,
        const ESM4::SwimBreathSettings& settings) const
    {
        if (!std::isfinite(duration) || duration < 0.f)
            throw std::invalid_argument("invalid native breath frame duration");
        ESM4::validateSwimBreathSettings(settings);
        const auto* life = findActorLife(values.mActor);
        if (!life)
            throw std::invalid_argument("native breath update requires initialized lifecycle");
        if (life->mPhase == ESM4::ActorLifePhase::Dead)
            return std::nullopt;
        const auto integer = [&](std::uint8_t value) {
            return values.mOwner == ESM4::ActorValueOwner::Player
                ? ESM4::composeIntegerActorValue(ESM4::combatBaseValue(values.mValues[value].mBase),
                    values.mValues[value].mModifiers, values.mOwner, values.mProcess)
                : nonPlayerInteger(values, value, findActorBase(values.mBase));
        };
        const float maximum = ESM4::swimBreathMaximum(integer(5), settings);
        if (!needsAir)
            return OblivionBreathUpdateResult{maximum, maximum, 0.f, false};
        // Original HighProcess construction/reset stores 20 at process+238.
        // Older native saves have no timer until this process is first adopted.
        const auto update = ESM4::updateSwimBreath(
            findActorBreath(values.mActor).value_or(20.f), maximum, duration, integer(55));
        const float damage = update.mDrowning
            ? ESM4::drowningDamage(ESM4::combatBaseValue(values.mValues[8].mBase), duration, settings) : 0.f;
        return OblivionBreathUpdateResult{update.mRemaining, maximum, damage, update.mDrowning};
    }

    void OblivionCombatService::publishBreathUpdate(const MWWorld::Ptr& actor, MWWorld::Player* player,
        const ESM::FormKey& key, const OblivionBreathUpdateResult& update, bool essential,
        const ESM4::EssentialRecoverySettings& recovery, const ESM4::PlayerDynamicBaseSettings& playerBase)
    {
        const auto found = mActorBreath.find(key);
        std::map<ESM::FormKey, float> prepared;
        if (found == mActorBreath.end())
        {
            // Allocate only on adoption, before the fallible Health transition.
            prepared = mActorBreath;
            prepared.emplace(key, update.mRemaining);
        }
        if (update.mDamage > 0.f)
        {
            if (player)
                changePlayerHealth(*player, -update.mDamage, {}, essential, recovery, playerBase);
            else
                changeNonPlayerHealth(actor, -update.mDamage, {}, essential, recovery);
        }
        // No callbacks occur inside the Health transaction. Existing timer writes
        // and prepared-map swaps cannot fail after its publication.
        if (found != mActorBreath.end())
            found->second = update.mRemaining;
        else
            mActorBreath.swap(prepared);
    }

    std::optional<OblivionBreathUpdateResult> OblivionCombatService::updateNonPlayerBreath(
        const MWWorld::Ptr& actor, float duration, bool needsAir, bool essential,
        const ESM4::SwimBreathSettings& settings, const ESM4::EssentialRecoverySettings& recovery)
    {
        const auto& values = nonPlayerValues(actor);
        const auto update = prepareBreathUpdate(values, duration, needsAir, settings);
        if (update)
            publishBreathUpdate(actor, nullptr, values.mActor, *update, essential, recovery, {});
        return update;
    }

    std::optional<OblivionBreathUpdateResult> OblivionCombatService::updatePlayerBreath(
        MWWorld::Player& player, float duration, bool needsAir, bool essential,
        const ESM4::SwimBreathSettings& settings, const ESM4::EssentialRecoverySettings& recovery,
        const ESM4::PlayerDynamicBaseSettings& playerBase, bool godMode)
    {
        const auto& values = playerValues();
        const auto update = prepareBreathUpdate(values, duration, needsAir && !godMode, settings);
        if (update)
            publishBreathUpdate(player.getPlayer(), &player, values.mActor, *update, essential, recovery, playerBase);
        return update;
    }

    bool OblivionCombatService::requestNonPlayerResourceCurrent(const MWWorld::Ptr& actor,
        std::uint8_t value, float requested, const ESM4::ActorValueCommandPolicy& policy,
        bool essential, const ESM4::EssentialRecoverySettings& recovery)
    {
        if (value < 8 || value > 10 || !std::isfinite(requested))
            throw std::invalid_argument("native resource request requires a finite Health/Magicka/Fatigue value");
        const auto& values = nonPlayerValues(actor);
        const float scale = value == 9
            ? ESM4::actorMagickaScale(ESM4::composeActorValue(values.mValues[40], values.mOwner, values.mProcess)) : 1.f;
        const float delta = static_cast<float>((double(requested) - getNonPlayerValue(actor, value)) / scale);
        if (!std::isfinite(delta))
            throw std::invalid_argument("native resource request delta overflow");
        if (value == 10 && delta < 0.f && !policy.mCanSpendFatigue)
            return false;
        if (value == 8)
            return changeNonPlayerHealth(actor, delta, {}, essential, recovery);
        changeNonPlayerValue(actor, value, ESM4::ActorValueModifier::Damage, delta);
        return true;
    }

    bool OblivionCombatService::requestPlayerResourceCurrent(MWWorld::Player& player,
        std::uint8_t value, float requested, bool godMode, bool essential,
        const ESM4::EssentialRecoverySettings& recovery, const ESM4::PlayerDynamicBaseSettings& settings)
    {
        if (value < 8 || value > 10 || !std::isfinite(requested))
            throw std::invalid_argument("native resource request requires a finite Health/Magicka/Fatigue value");
        auto values = playerValues();
        preparePlayerValues(values, settings);
        const float current = ESM4::composeActorValue(values.mValues[value], values.mOwner, values.mProcess);
        const float delta = static_cast<float>(double(requested) - current);
        if (!std::isfinite(delta))
            throw std::invalid_argument("native resource request delta overflow");
        if (godMode && delta < 0.f)
            return false;
        if (value == 8)
            return changePlayerHealth(player, delta, {}, essential, recovery, settings, godMode);
        changePlayerValue(player, value, ESM4::ActorValueModifier::Damage, delta, settings);
        return true;
    }

    OblivionActorValueCommandResult OblivionCombatService::executeNonPlayerValueCommand(
        const MWWorld::Ptr& actor, std::uint8_t value, ESM4::ActorValueCommand command,
        ESM4::ActorValueCommandSource source, std::int32_t requested,
        const ESM4::ActorValueCommandPolicy& policy, std::span<const MWWorld::Ptr> residents,
        bool essential, const ESM4::EssentialRecoverySettings& recoverySettings)
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
        if (change->mHealthReaction)
        {
            auto candidate = nonPlayerValues(actor);
            candidate.mValues[8] = ESM4::changeActorValueModifier(
                candidate.mValues[8], candidate.mOwner, 8, change->mModifier, change->mDelta);
            publishHealthChange(actor, std::move(candidate), essential, recoverySettings, false);
        }
        else
            changeNonPlayerValue(actor, value, change->mModifier, change->mDelta);
        return {true, change->mHealthReaction ? std::optional(change->mDelta) : std::nullopt};
    }

    OblivionActorValueCommandResult OblivionCombatService::executeUnloadedValueCommand(
        const ESM::FormKey& actor, const MWWorld::ESMStore& store, std::uint8_t value,
        ESM4::ActorValueCommand command, ESM4::ActorValueCommandSource source,
        std::int32_t requested, const ESM4::ActorValueCommandPolicy& policy,
        std::span<const MWWorld::Ptr> residents, bool essential,
        const ESM4::EssentialRecoverySettings& recoverySettings)
    {
        validateNonPlayerQuery(value);
        auto candidate = nonPlayerValues(actor);
        const bool creature = nonPlayerContentIsCreature(candidate, store);
        if (source != ESM4::ActorValueCommandSource::Script && source != ESM4::ActorValueCommandSource::Console)
            throw std::invalid_argument("invalid native actor-value command source");
        for (const auto& ptr : residents)
            if (!ptr.isEmpty() && ptr.getCellRef().getFormKey() == actor)
                throw std::invalid_argument("unloaded native command target has a resident projection");
        if (command == ESM4::ActorValueCommand::Set)
        {
            setNonPlayerBaseValue(candidate, creature ? ESM4::ActorBaseKind::Creature : ESM4::ActorBaseKind::Npc,
                value, requested, residents);
            return {true, std::nullopt};
        }
        if (!residents.empty())
            throw std::invalid_argument("unloaded modifier command does not affect resident siblings");
        const auto change = ESM4::prepareActorValueModifierCommand(ESM4::ActorValueOwner::NonPlayer,
            value, command, source, requested,
            command == ESM4::ActorValueCommand::Force ? getNonPlayerValue(actor, value, store) : 0.f, policy);
        if (!change)
            return {false, std::nullopt};
        value = nonPlayerValueIndex(creature, value);
        candidate.mValues[value] = ESM4::changeActorValueModifier(
            candidate.mValues[value], candidate.mOwner, value, change->mModifier, change->mDelta);
        if (change->mHealthReaction)
            publishHealthChange({}, std::move(candidate), essential, recoverySettings, false);
        else
        {
            candidate.validate();
            actorProjection(candidate, findActorBase(candidate.mBase), findActorLife(actor));
            std::swap(mActorValues.at(actor), candidate);
        }
        return {true, change->mHealthReaction ? std::optional(change->mDelta) : std::nullopt};
    }

    OblivionActorValueCommandResult OblivionCombatService::executePlayerValueCommand(
        MWWorld::Player& player, std::uint8_t value, ESM4::ActorValueCommand command,
        ESM4::ActorValueCommandSource source, std::int32_t requested,
        const ESM4::ActorValueCommandPolicy& policy, const ESM4::PlayerDynamicBaseSettings& settings,
        bool essential, const ESM4::EssentialRecoverySettings& recoverySettings)
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
        if (change->mHealthReaction)
        {
            auto candidate = playerValues();
            candidate.mValues[8] = ESM4::changeActorValueModifier(
                candidate.mValues[8], candidate.mOwner, 8, change->mModifier, change->mDelta);
            preparePlayerValues(candidate, settings);
            publishHealthChange(player.getPlayer(), std::move(candidate), essential, recoverySettings, policy.mGodMode);
        }
        else
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

    void OblivionCombatService::preparePlayerValues(ESM4::RuntimeActorValues& values,
        const ESM4::PlayerDynamicBaseSettings& settings) const
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
    }

    void OblivionCombatService::publishPlayerValues(MWWorld::Player& player, ESM4::RuntimeActorValues values,
        const ESM4::PlayerDynamicBaseSettings& settings)
    {
        preparePlayerValues(values, settings);
        const auto ptr = player.getPlayer();
        OblivionActorProjection prepared(ptr.getClass().getNpcStats(ptr),
            actorProjection(values, nullptr, findActorLife(values.mActor)));
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
            candidate.mValues[value], candidate.mOwner, value, modifier, delta);
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
        auto candidate = playerValues();
        const auto ptr = player.getPlayer();
        if (ptr.getClass().getCreatureStats(ptr).isDead())
            return;
        preparePlayerValues(candidate, baseSettings);
        const float delta = fatigueRestoration(candidate, duration, settings);
        if (delta > 0)
            candidate.mValues[10] = ESM4::changeActorValueModifier(
                candidate.mValues[10], candidate.mOwner, 10, ESM4::ActorValueModifier::Damage, delta);
        if (candidate != playerValues())
            publishPlayerValues(player, std::move(candidate), baseSettings);
    }

    void OblivionCombatService::regenerateNonPlayerMagicka(const MWWorld::Ptr& actor, float duration,
        bool hasActiveMagicItem, const ESM4::MagickaRegenerationSettings& settings)
    {
        const auto& values = nonPlayerValues(actor);
        if (actor.getClass().getCreatureStats(actor).isDead())
            return;
        const float delta = magickaRestoration(values, duration, hasActiveMagicItem, settings, findActorBase(values.mBase));
        if (delta > 0)
            changeNonPlayerValue(actor, 9, ESM4::ActorValueModifier::Damage, delta);
    }

    void OblivionCombatService::regeneratePlayerMagicka(MWWorld::Player& player, float duration,
        bool hasActiveMagicItem, const ESM4::MagickaRegenerationSettings& settings,
        const ESM4::PlayerDynamicBaseSettings& baseSettings)
    {
        auto candidate = playerValues();
        const auto ptr = player.getPlayer();
        if (ptr.getClass().getCreatureStats(ptr).isDead())
            return;
        preparePlayerValues(candidate, baseSettings);
        const float delta = magickaRestoration(candidate, duration, hasActiveMagicItem, settings);
        if (delta > 0)
            candidate.mValues[9] = ESM4::changeActorValueModifier(
                candidate.mValues[9], candidate.mOwner, 9, ESM4::ActorValueModifier::Damage, delta);
        if (candidate != playerValues())
            publishPlayerValues(player, std::move(candidate), baseSettings);
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
        if (ptr.getClass().getCreatureStats(ptr).isDead())
            return;
        preparePlayerValues(candidate, settings.mPlayerBase);
        restoreResources(candidate, input, settings);
        if (candidate != playerValues())
            publishPlayerValues(player, std::move(candidate), settings.mPlayerBase);
    }

    void OblivionCombatService::restoreResourceBatch(MWWorld::Player& player,
        std::span<const OblivionActorRestoration> updates, std::span<const MWWorld::Ptr> residents,
        const OblivionRestorationSettings& settings, std::optional<float> completedManagerTime)
    {
        ESM4::validateMagickaRegenerationSettings(settings.mMagicka);
        ESM4::validateFatigueRegenerationSettings(settings.mFatigue);
        auto candidates = mActorValues;
        if (completedManagerTime && (!std::isfinite(*completedManagerTime) || *completedManagerTime > 100000.f))
            throw std::invalid_argument("invalid native completed manager time");
        auto updateTimes = mActorUpdateTimes;
        std::set<ESM::FormKey> affected;
        std::optional<OblivionActorProjection> playerView;
        for (const auto& update : updates)
        {
            if (!affected.insert(update.mActor).second || update.mUpdates.empty())
                throw std::invalid_argument("duplicate or empty native resource update");
            const auto found = candidates.find(update.mActor);
            if (found == candidates.end())
                throw std::invalid_argument("native resource update requires registered actor values");
            auto& values = found->second;
            const auto* life = findActorLife(update.mActor);
            if (!life)
                throw std::invalid_argument("native resource update requires initialized lifecycle");
            for (const auto& step : update.mUpdates)
                if (!std::isfinite(step.mDuration) || step.mDuration < 0.f)
                    throw std::invalid_argument("invalid native resource update duration");
            const bool isPlayer = values.mOwner == ESM4::ActorValueOwner::Player;
            const auto* base = isPlayer ? nullptr : findActorBase(values.mBase);
            if (life->mPhase != ESM4::ActorLifePhase::Dead)
            {
                if (isPlayer)
                    preparePlayerValues(values, settings.mPlayerBase);
                // Preserve each native call's float store and sparse modifier
                // semantics; summing durations is not generally equivalent.
                for (const auto& step : update.mUpdates)
                    restoreResources(values, step, settings, base);
            }
            values.validate();
            if (completedManagerTime)
                updateTimes.insert_or_assign(update.mActor, *completedManagerTime);
            const auto projection = actorProjection(values, base, life);
            if (isPlayer)
                playerView.emplace(player.getPlayer().getClass().getNpcStats(player.getPlayer()), projection);
        }
        std::set<ESM::FormKey> identities;
        std::list<PreparedNonPlayerView> prepared;
        for (const auto& ptr : residents)
        {
            const auto& old = nonPlayerValues(ptr);
            if (!affected.contains(old.mActor) || !identities.insert(old.mActor).second)
                throw std::invalid_argument("unaffected or duplicate native resource resident");
            prepared.emplace_back(ptr, candidates.at(old.mActor), findActorBase(old.mBase), findActorLife(old.mActor));
        }
        // All fallible work precedes the authority/projection publication.
        mActorValues.swap(candidates);
        if (completedManagerTime)
        {
            mActorUpdateTimes.swap(updateTimes);
            mActorManagerTime = *completedManagerTime;
        }
        if (playerView)
            playerView->commit();
        for (auto& view : prepared)
            view.commit();
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
        if (ptr.getClass().getCreatureStats(ptr).isDead())
            return;
        preparePlayerValues(candidate, settings.mPlayerBase);
        updateFatigue(candidate, input, settings);
        if (candidate != playerValues())
            publishPlayerValues(player, std::move(candidate), settings.mPlayerBase);
    }

    void OblivionCombatService::updateNonPlayerFrameResources(const MWWorld::Ptr& actor,
        const OblivionFatigueUpdate& input, bool hasActiveMagicItem, const OblivionFrameSettings& settings)
    {
        auto candidate = nonPlayerValues(actor);
        if (!actor.getClass().getCreatureStats(actor).isDead()
            && updateFrameResources(candidate, input, hasActiveMagicItem, settings, findActorBase(candidate.mBase)))
            publishNonPlayerValues(actor, std::move(candidate));
    }

    void OblivionCombatService::updatePlayerFrameResources(MWWorld::Player& player,
        const OblivionFatigueUpdate& input, bool hasActiveMagicItem, const OblivionFrameSettings& settings)
    {
        auto candidate = playerValues();
        const auto ptr = player.getPlayer();
        if (ptr.getClass().getCreatureStats(ptr).isDead())
            return;
        preparePlayerValues(candidate, settings.mFatigue.mPlayerBase);
        updateFrameResources(candidate, input, hasActiveMagicItem, settings);
        if (candidate != playerValues())
            publishPlayerValues(player, std::move(candidate), settings.mFatigue.mPlayerBase);
    }

    void OblivionCombatService::updateNonPlayerFrameResourcesFromClock(const MWWorld::Ptr& actor,
        const OblivionActorMovement& movement, bool hasActiveMagicItem, const OblivionFrameSettings& settings)
    {
        const auto key = nonPlayerValues(actor).mActor;
        updateWithActorClock(mActorUpdateTimes, key, mActorManagerTime, [&](float elapsed) {
            updateNonPlayerFrameResources(actor, {elapsed, movement.mEncumbrance, movement.mRunning, movement.mCanSpend},
                hasActiveMagicItem, settings);
        });
    }

    void OblivionCombatService::updatePlayerFrameResourcesFromClock(MWWorld::Player& player,
        const OblivionActorMovement& movement, bool hasActiveMagicItem, const OblivionFrameSettings& settings)
    {
        const auto key = playerValues().mActor;
        updateWithActorClock(mActorUpdateTimes, key, mActorManagerTime, [&](float elapsed) {
            updatePlayerFrameResources(player, {elapsed, movement.mEncumbrance, movement.mRunning, movement.mCanSpend},
                hasActiveMagicItem, settings);
        });
    }

    void OblivionCombatService::spendPlayerJumpFatigue(MWWorld::Player& player,
        std::int32_t encumbrance, bool canSpend, const OblivionFatigueSettings& settings)
    {
        auto candidate = playerValues();
        const auto ptr = player.getPlayer();
        if (ptr.getClass().getCreatureStats(ptr).isDead())
            return;
        preparePlayerValues(candidate, settings.mPlayerBase);
        spendJumpFatigue(candidate, encumbrance, canSpend, settings);
        if (candidate != playerValues())
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

    void OblivionCombatService::publishNonPlayerLife(const MWWorld::Ptr& actor, ESM4::RuntimeActorLife life)
    {
        const auto& values = nonPlayerValues(actor);
        PreparedNonPlayerView prepared(actor, values, findActorBase(values.mBase), &life);
        const bool terminal = life.mPhase == ESM4::ActorLifePhase::Dead;
        const auto found = mActorLife.find(life.mActor);
        if (found == mActorLife.end())
        {
            const auto key = life.mActor;
            mActorLife.emplace(key, std::move(life));
        }
        else
        {
            static_assert(std::is_nothrow_swappable_v<ESM4::RuntimeActorLife>);
            std::swap(found->second, life);
        }
        prepared.commit();
        if (terminal)
            stopCombat(values.mActor);
    }

    void OblivionCombatService::publishPlayerLife(MWWorld::Player& player, ESM4::RuntimeActorLife life)
    {
        const auto& values = playerValues();
        const auto ptr = player.getPlayer();
        OblivionActorProjection prepared(ptr.getClass().getNpcStats(ptr), actorProjection(values, nullptr, &life));
        const bool terminal = life.mPhase == ESM4::ActorLifePhase::Dead;
        const auto found = mActorLife.find(life.mActor);
        if (found == mActorLife.end())
        {
            const auto key = life.mActor;
            mActorLife.emplace(key, std::move(life));
        }
        else
            std::swap(found->second, life);
        prepared.commit();
        if (terminal)
            stopCombat(values.mActor);
    }

    std::optional<OblivionCombatService::PreparedLifeTransition> OblivionCombatService::prepareLifeTransition(
        const ESM4::RuntimeActorLife& life, bool afterRevival) const
    {
        life.validate();
        const auto* previous = findActorLife(life.mActor);
        if (!previous || previous->mBase != life.mBase)
            throw std::invalid_argument("native lifecycle transition requires initialized matching authority");
        // Resurrection visits Alive before its Health writer, even if the
        // prepared final phase is Dead again. That is a new terminal entry.
        if (!afterRevival && previous->mPhase == life.mPhase)
            return std::nullopt;
        PreparedLifeTransition prepared{mPendingDeathEvents, mDeathCounts};
        if (life.mPhase == ESM4::ActorLifePhase::Dead)
        {
            if (mNextDeathEvent == std::numeric_limits<std::uint64_t>::max())
                throw std::overflow_error("native death event namespace exhausted");
            prepared.mEvents.push_back({mNextDeathEvent, life.mActor, life.mKiller});
            auto& count = prepared.mCounts[life.mBase];
            count = static_cast<std::uint16_t>(static_cast<std::uint32_t>(count) + 1);
        }
        return prepared;
    }

    bool OblivionCombatService::transitionNonPlayerLife(const MWWorld::Ptr& actor, ESM4::RuntimeActorLife life)
    {
        const auto& values = nonPlayerValues(actor);
        if (values.mActor != life.mActor || values.mBase != life.mBase)
            throw std::invalid_argument("native lifecycle transition identity mismatch");
        auto events = prepareLifeTransition(life);
        if (!events)
            return false;
        const bool death = life.mPhase == ESM4::ActorLifePhase::Dead;
        publishNonPlayerLife(actor, std::move(life));
        // All potentially throwing preparation is finished before publication.
        mPendingDeathEvents.swap(events->mEvents);
        mDeathCounts.swap(events->mCounts);
        if (death)
            ++mNextDeathEvent;
        return true;
    }

    bool OblivionCombatService::transitionPlayerLife(MWWorld::Player& player, ESM4::RuntimeActorLife life)
    {
        const auto& values = playerValues();
        if (values.mActor != life.mActor || values.mBase != life.mBase)
            throw std::invalid_argument("native lifecycle transition identity mismatch");
        auto events = prepareLifeTransition(life);
        if (!events)
            return false;
        const bool death = life.mPhase == ESM4::ActorLifePhase::Dead;
        publishPlayerLife(player, std::move(life));
        mPendingDeathEvents.swap(events->mEvents);
        mDeathCounts.swap(events->mCounts);
        if (death)
            ++mNextDeathEvent;
        return true;
    }

    void OblivionCombatService::publishHealthChange(const MWWorld::Ptr& actor,
        ESM4::RuntimeActorValues values, bool essential,
        const ESM4::EssentialRecoverySettings& settings, bool godMode, const ESM::FormKey& source)
    {
        const auto found = mActorLife.find(values.mActor);
        if (found == mActorLife.end())
            throw std::invalid_argument("native Health reaction requires initialized lifecycle");
        const bool player = values.mOwner == ESM4::ActorValueOwner::Player;
        const auto* base = findActorBase(values.mBase);
        const float current = player
            ? ESM4::composeActorValue(values.mValues[8], values.mOwner, values.mProcess)
            : nonPlayerFloat(values, 8, base);
        auto life = found->second;
        if (life.mPhase == ESM4::ActorLifePhase::Alive && current < 1.f)
        {
            life.mKiller = source;
            life.mPhase = essential ? ESM4::ActorLifePhase::EssentialUnconscious : ESM4::ActorLifePhase::Dead;
            if (essential)
            {
                const auto recovery = ESM4::essentialRecoveryHealth(
                    ESM4::combatBaseValue(values.mValues[8].mBase), current, settings);
                life.mRecoveryRemaining = settings.mDelay;
                if (!player || !godMode || recovery.mAdjustment >= 0.f)
                    values.mValues[8] = ESM4::changeActorValueModifier(values.mValues[8], values.mOwner, 8,
                        ESM4::ActorValueModifier::Damage, recovery.mAdjustment);
            }
        }
        values.validate();
        life.validate();
        auto transition = prepareLifeTransition(life);
        std::optional<OblivionActorProjection> playerView;
        std::optional<PreparedNonPlayerView> actorView;
        if (player)
            playerView.emplace(actor.getClass().getNpcStats(actor), actorProjection(values, nullptr, &life));
        else if (!actor.isEmpty())
            actorView.emplace(actor, values, base, &life);
        else
            actorProjection(values, base, &life);
        const bool terminal = life.mPhase == ESM4::ActorLifePhase::Dead;
        std::swap(mActorValues.at(values.mActor), values);
        std::swap(found->second, life);
        if (playerView)
            playerView->commit();
        else if (actorView)
            actorView->commit();
        if (terminal)
            stopCombat(found->first);
        if (transition)
        {
            mPendingDeathEvents.swap(transition->mEvents);
            mDeathCounts.swap(transition->mCounts);
            if (terminal)
                ++mNextDeathEvent;
        }
    }

    bool OblivionCombatService::enterNonPlayerDeath(const MWWorld::Ptr& actor,
        const ESM::FormKey& killer, bool essential, const ESM4::EssentialRecoverySettings& settings, bool healthGate)
    {
        auto candidate = nonPlayerValues(actor);
        const auto found = mActorLife.find(candidate.mActor);
        if (found == mActorLife.end())
            throw std::invalid_argument("native Health reaction requires initialized lifecycle");
        const auto* base = findActorBase(candidate.mBase);
        const float current = nonPlayerFloat(candidate, 8, base);
        if (found->second.mPhase != ESM4::ActorLifePhase::Alive || (healthGate && current >= 1.f))
            return false;
        auto life = found->second;
        life.mKiller = killer;
        life.mPhase = essential ? ESM4::ActorLifePhase::EssentialUnconscious : ESM4::ActorLifePhase::Dead;
        if (!essential)
            return transitionNonPlayerLife(actor, std::move(life));
        const auto recovery = ESM4::essentialRecoveryHealth(
            ESM4::combatBaseValue(candidate.mValues[8].mBase), current, settings);
        life.mRecoveryRemaining = settings.mDelay;
        candidate.mValues[8] = ESM4::changeActorValueModifier(candidate.mValues[8], candidate.mOwner, 8,
            ESM4::ActorValueModifier::Damage, recovery.mAdjustment);
        candidate.validate();
        PreparedNonPlayerView prepared(actor, candidate, base, &life);
        std::swap(mActorValues.at(candidate.mActor), candidate);
        std::swap(found->second, life);
        prepared.commit();
        return true;
    }

    bool OblivionCombatService::enterPlayerDeath(MWWorld::Player& player,
        const ESM::FormKey& killer, bool essential, const ESM4::EssentialRecoverySettings& settings, bool healthGate, bool godMode)
    {
        auto candidate = playerValues();
        const auto found = mActorLife.find(candidate.mActor);
        if (found == mActorLife.end())
            throw std::invalid_argument("native Health reaction requires initialized lifecycle");
        const float current = ESM4::composeActorValue(candidate.mValues[8], candidate.mOwner, candidate.mProcess);
        if (found->second.mPhase != ESM4::ActorLifePhase::Alive || (healthGate && current >= 1.f))
            return false;
        auto life = found->second;
        life.mKiller = killer;
        life.mPhase = essential ? ESM4::ActorLifePhase::EssentialUnconscious : ESM4::ActorLifePhase::Dead;
        if (!essential)
            return transitionPlayerLife(player, std::move(life));
        const auto recovery = ESM4::essentialRecoveryHealth(
            ESM4::combatBaseValue(candidate.mValues[8].mBase), current, settings);
        life.mRecoveryRemaining = settings.mDelay;
        if (!godMode || recovery.mAdjustment >= 0.f)
            candidate.mValues[8] = ESM4::changeActorValueModifier(candidate.mValues[8], candidate.mOwner, 8,
                ESM4::ActorValueModifier::Damage, recovery.mAdjustment);
        candidate.validate();
        const auto ptr = player.getPlayer();
        OblivionActorProjection prepared(ptr.getClass().getNpcStats(ptr), actorProjection(candidate, nullptr, &life));
        std::swap(mActorValues.at(candidate.mActor), candidate);
        std::swap(found->second, life);
        prepared.commit();
        return true;
    }

    bool OblivionCombatService::reactNonPlayerHealth(const MWWorld::Ptr& actor,
        const ESM::FormKey& killer, bool essential, const ESM4::EssentialRecoverySettings& settings)
    {
        return enterNonPlayerDeath(actor, killer, essential, settings, true);
    }

    bool OblivionCombatService::reactPlayerHealth(MWWorld::Player& player,
        const ESM::FormKey& killer, bool essential, const ESM4::EssentialRecoverySettings& settings, bool godMode)
    {
        return enterPlayerDeath(player, killer, essential, settings, true, godMode);
    }

    bool OblivionCombatService::killNonPlayer(const MWWorld::Ptr& actor,
        const ESM::FormKey& killer, bool essential, const ESM4::EssentialRecoverySettings& settings)
    {
        return enterNonPlayerDeath(actor, killer, essential, settings, false);
    }

    bool OblivionCombatService::killPlayer(MWWorld::Player& player,
        const ESM::FormKey& killer, bool essential, const ESM4::EssentialRecoverySettings& settings, bool godMode)
    {
        return enterPlayerDeath(player, killer, essential, settings, false, godMode);
    }

    void OblivionCombatService::resetNonPlayerForResurrection(const MWWorld::Ptr& actor)
    {
        auto values = nonPlayerValues(actor);
        const auto found = mActorLife.find(values.mActor);
        if (found == mActorLife.end())
            throw std::invalid_argument("native resurrection requires initialized lifecycle");
        auto life = found->second;
        prepareResurrectionReset(values, life);
        PreparedNonPlayerView prepared(actor, values, findActorBase(values.mBase), &life);
        std::swap(mActorValues.at(values.mActor), values);
        std::swap(found->second, life);
        prepared.commit();
    }

    void OblivionCombatService::resetPlayerForResurrection(MWWorld::Player& player)
    {
        auto values = playerValues();
        const auto found = mActorLife.find(values.mActor);
        if (found == mActorLife.end())
            throw std::invalid_argument("native resurrection requires initialized lifecycle");
        auto life = found->second;
        prepareResurrectionReset(values, life);
        const auto ptr = player.getPlayer();
        OblivionActorProjection prepared(ptr.getClass().getNpcStats(ptr), actorProjection(values, nullptr, &life));
        std::swap(mActorValues.at(values.mActor), values);
        std::swap(found->second, life);
        prepared.commit();
    }

    void OblivionCombatService::reviveNonPlayerPreservingState(const MWWorld::Ptr& actor, bool essential,
        const ESM4::EssentialRecoverySettings& settings)
    {
        auto values = nonPlayerValues(actor);
        const auto found = mActorLife.find(values.mActor);
        if (found == mActorLife.end())
            throw std::invalid_argument("native resurrection requires initialized lifecycle");
        const auto* base = findActorBase(values.mBase);
        const auto baseHealth = ESM4::combatBaseValue(values.mValues[8].mBase);
        const float delta = ESM4::forceActorValueDelta(baseHealth, nonPlayerFloat(values, 8, base));
        auto life = found->second;
        life.mPhase = ESM4::ActorLifePhase::Alive;
        life.mRecoveryRemaining = 0;
        life.mKiller = {};
        values.mValues[8] = ESM4::changeActorValueModifier(values.mValues[8], values.mOwner, 8,
            ESM4::ActorValueModifier::Damage, delta);
        const float current = nonPlayerFloat(values, 8, base);
        if (delta < 0.f && current < 1.f)
        {
            life.mPhase = essential ? ESM4::ActorLifePhase::EssentialUnconscious : ESM4::ActorLifePhase::Dead;
            if (essential)
            {
                const auto recovery = ESM4::essentialRecoveryHealth(baseHealth, current, settings);
                life.mRecoveryRemaining = settings.mDelay;
                values.mValues[8] = ESM4::changeActorValueModifier(values.mValues[8], values.mOwner, 8,
                    ESM4::ActorValueModifier::Damage, recovery.mAdjustment);
            }
        }
        values.validate();
        life.validate();
        auto terminal = life.mPhase == ESM4::ActorLifePhase::Dead ? prepareLifeTransition(life, true) : std::nullopt;
        auto& stats = actor.getClass().getCreatureStats(actor);
        PreparedNonPlayerView prepared(actor, values, base, &life);
        std::swap(mActorValues.at(values.mActor), values);
        std::swap(found->second, life);
        prepared.commit();
        // Revival passes through Alive even when its Health write immediately kills again.
        stats.setDeathAnimationFinished(false);
        if (terminal)
        {
            stopCombat(found->first);
            mPendingDeathEvents.swap(terminal->mEvents);
            mDeathCounts.swap(terminal->mCounts);
            ++mNextDeathEvent;
        }
    }

    void OblivionCombatService::prepareEssentialWake(ESM4::RuntimeActorValues& values,
        ESM4::RuntimeActorLife& life, bool essential, bool godMode,
        const ESM4::EssentialRecoverySettings& settings) const
    {
        const bool player = values.mOwner == ESM4::ActorValueOwner::Player;
        const auto current = [&]() {
            return player ? ESM4::composeActorValue(values.mValues[8], values.mOwner, values.mProcess)
                : nonPlayerFloat(values, 8, findActorBase(values.mBase));
        };
        const auto restoreHealth = [&]() {
            const auto recovery = ESM4::essentialRecoveryHealth(
                ESM4::combatBaseValue(values.mValues[8].mBase), current(), settings);
            if (player && godMode && recovery.mAdjustment < 0.f)
                return false;
            values.mValues[8] = ESM4::changeActorValueModifier(values.mValues[8], values.mOwner, 8,
                ESM4::ActorValueModifier::Damage, recovery.mAdjustment);
            return recovery.mAdjustment < 0.f;
        };
        // Original wake changes life to Alive before its Damage-channel write.
        // A negative write can immediately enter death again; prepare the whole
        // result before exposing any intermediate life/value state to readers.
        life.mPhase = ESM4::ActorLifePhase::Alive;
        life.mRecoveryRemaining = 0;
        life.mKiller = {};
        if (restoreHealth() && current() < 1.f)
        {
            life.mPhase = essential ? ESM4::ActorLifePhase::EssentialUnconscious : ESM4::ActorLifePhase::Dead;
            if (essential)
            {
                life.mRecoveryRemaining = settings.mDelay;
                restoreHealth(); // The new unconscious phase suppresses another health reaction.
            }
        }
        values.validate();
        life.validate();
    }

    bool OblivionCombatService::advanceNonPlayerEssentialRecovery(const MWWorld::Ptr& actor,
        float frameSeconds, std::int8_t knockedState, bool essential,
        const ESM4::EssentialRecoverySettings& settings)
    {
        auto values = nonPlayerValues(actor);
        const auto found = mActorLife.find(values.mActor);
        if (found == mActorLife.end())
            throw std::invalid_argument("native essential recovery requires initialized lifecycle");
        const auto tick = ESM4::advanceEssentialRecovery(found->second.mRecoveryRemaining, frameSeconds,
            found->second.mPhase == ESM4::ActorLifePhase::EssentialUnconscious, knockedState);
        if (!tick.mRecover)
        {
            found->second.mRecoveryRemaining = tick.mRemaining;
            return false;
        }
        auto life = found->second;
        prepareEssentialWake(values, life, essential, false, settings);
        auto terminal = life.mPhase == ESM4::ActorLifePhase::Dead ? prepareLifeTransition(life) : std::nullopt;
        PreparedNonPlayerView prepared(actor, values, findActorBase(values.mBase), &life);
        std::swap(mActorValues.at(values.mActor), values);
        std::swap(found->second, life);
        prepared.commit();
        if (terminal)
        {
            stopCombat(found->first);
            mPendingDeathEvents.swap(terminal->mEvents);
            mDeathCounts.swap(terminal->mCounts);
            ++mNextDeathEvent;
        }
        return true;
    }

    bool OblivionCombatService::advancePlayerEssentialRecovery(MWWorld::Player& player,
        float frameSeconds, std::int8_t knockedState, bool essential,
        const ESM4::EssentialRecoverySettings& settings, bool godMode)
    {
        auto values = playerValues();
        const auto found = mActorLife.find(values.mActor);
        if (found == mActorLife.end())
            throw std::invalid_argument("native essential recovery requires initialized lifecycle");
        const auto tick = ESM4::advanceEssentialRecovery(found->second.mRecoveryRemaining, frameSeconds,
            found->second.mPhase == ESM4::ActorLifePhase::EssentialUnconscious, knockedState);
        if (!tick.mRecover)
        {
            found->second.mRecoveryRemaining = tick.mRemaining;
            return false;
        }
        auto life = found->second;
        prepareEssentialWake(values, life, essential, godMode, settings);
        auto terminal = life.mPhase == ESM4::ActorLifePhase::Dead ? prepareLifeTransition(life) : std::nullopt;
        const auto ptr = player.getPlayer();
        OblivionActorProjection prepared(ptr.getClass().getNpcStats(ptr), actorProjection(values, nullptr, &life));
        std::swap(mActorValues.at(values.mActor), values);
        std::swap(found->second, life);
        prepared.commit();
        if (terminal)
        {
            stopCombat(found->first);
            mPendingDeathEvents.swap(terminal->mEvents);
            mDeathCounts.swap(terminal->mCounts);
            ++mNextDeathEvent;
        }
        return true;
    }

    const ESM4::RuntimeActorLife* OblivionCombatService::findActorLife(const ESM::FormKey& actor) const
    {
        const auto found = mActorLife.find(actor);
        return found == mActorLife.end() ? nullptr : &found->second;
    }

    std::int32_t OblivionCombatService::getDeadCount(const ESM::FormKey& base) const
    {
        const auto found = mDeathCounts.find(base);
        if (found == mDeathCounts.end())
            return 0;
        const std::int32_t count = found->second;
        return count >= 32768 ? count - 65536 : count;
    }

    std::optional<ESM4::RuntimeActorDeathEvent> OblivionCombatService::takeNextDeathEvent()
    {
        if (mPendingDeathEvents.empty())
            return std::nullopt;
        static_assert(std::is_nothrow_move_constructible_v<ESM4::RuntimeActorDeathEvent>);
        auto event = std::move(mPendingDeathEvents.front());
        mPendingDeathEvents.pop_front();
        return event;
    }

    std::optional<float> OblivionCombatService::findActorBreath(const ESM::FormKey& actor) const
    {
        const auto found = mActorBreath.find(actor);
        return found == mActorBreath.end() ? std::nullopt : std::optional(found->second);
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
        if (state.mVersion < 12 && (!mActorLife.empty() || !mPendingDeathEvents.empty() || mNextDeathEvent != 1))
            throw std::invalid_argument("native actor lifecycle requires an Oblivion v12+ save");
        if (state.mVersion < 13 && !mDeathCounts.empty())
            throw std::invalid_argument("native death counts require an Oblivion v13+ save");
        if (state.mVersion < 14 && !mActorBreath.empty())
            throw std::invalid_argument("native actor breath requires an Oblivion v14+ save");
        if (state.mVersion < 15 && !mCombatOpponents.empty())
            throw std::invalid_argument("native combat engagements require an Oblivion v15+ save");
        if (state.mVersion < 16 && (mActorManagerTime != 0.f || !mActorUpdateTimes.empty()))
            throw std::invalid_argument("native actor clocks require an Oblivion v16+ save");
        decltype(state.mNativeCombatEngagements) engagements;
        for (const auto& [actor, opponents] : mCombatOpponents)
            for (const auto& opponent : opponents)
                if (actor < opponent)
                    engagements.emplace(actor, opponent);
        auto breath = mActorBreath;
        auto updateTimes = mActorUpdateTimes;
        auto deathCounts = mDeathCounts;
        std::vector<ESM4::RuntimeActorLife> lives;
        lives.reserve(mActorLife.size());
        for (const auto& [key, life] : mActorLife)
            lives.push_back(life);
        std::vector<ESM4::RuntimeActorDeathEvent> events(mPendingDeathEvents.begin(), mPendingDeathEvents.end());
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
        state.mNativeActorLife.swap(lives);
        state.mNativeDeathCounts.swap(deathCounts);
        state.mNativeActorBreath.swap(breath);
        state.mNativeActorManagerTime = mActorManagerTime;
        state.mNativeActorUpdateTimes.swap(updateTimes);
        state.mNativeCombatEngagements.swap(engagements);
        state.mPendingDeathEvents.swap(events);
        state.mNextDeathEvent = mNextDeathEvent;
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
        for (const auto& [base, count] : state.mNativeDeathCounts)
            if (base != ESM::FormKey::dynamic("player-base", 1))
                nativeBaseIsCreature(base, store); // Requires an actual winning NPC/creature base.
        const auto validateLifeIdentity = [&](const ESM::FormKey& actor, const ESM::FormKey& base) {
            if (actor == state.mPlayer.mReference)
            {
                if (base != ESM::FormKey::dynamic("player-base", 1) && nativeBaseIsCreature(base, store))
                    throw std::invalid_argument("native player life requires an NPC base");
                return;
            }
            ESM4::RuntimeActorValues identity;
            identity.mActor = actor;
            identity.mBase = base;
            nonPlayerContentIsCreature(identity, store);
        };
        std::map<ESM::FormKey, ESM::FormKey> references;
        for (const auto& reference : state.mReferences)
            references.emplace(reference.mKey, reference.mBase);
        const auto validateSource = [&](const ESM::FormKey& source) {
            if (!source.isNull() && source != state.mPlayer.mReference)
                validateLifeIdentity(source, references.at(source));
        };
        for (const auto& life : state.mNativeActorLife)
        {
            validateLifeIdentity(life.mActor, life.mBase);
            validateSource(life.mKiller);
        }
        for (const auto& event : state.mPendingDeathEvents)
            validateSource(event.mKiller);
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
        decltype(mCombatOpponents) opponents;
        for (const auto& [first, second] : state.mNativeCombatEngagements)
        {
            opponents[first].insert(second);
            opponents[second].insert(first);
        }
        auto deathCounts = state.mNativeDeathCounts;
        auto breath = state.mNativeActorBreath;
        auto updateTimes = state.mNativeActorUpdateTimes;
        std::map<ESM::FormKey, ESM4::RuntimeActorLife> lives;
        for (const auto& life : state.mNativeActorLife)
            lives.emplace(life.mActor, life);
        std::deque<ESM4::RuntimeActorDeathEvent> events(state.mPendingDeathEvents.begin(), state.mPendingDeathEvents.end());
        mActions = std::move(actions);
        mActorValues.swap(actors);
        mActorBases.swap(bases);
        mActorLife.swap(lives);
        mDeathCounts.swap(deathCounts);
        mActorBreath.swap(breath);
        mActorManagerTime = state.mNativeActorManagerTime;
        mActorUpdateTimes.swap(updateTimes);
        mCombatOpponents.swap(opponents);
        mPendingDeathEvents.swap(events);
        mNextDeathEvent = state.mNextDeathEvent;
    }
}
