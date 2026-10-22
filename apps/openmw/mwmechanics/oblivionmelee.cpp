#include "oblivionmelee.hpp"

#include "oblivioncombat.hpp"
#include "oblivionai.hpp"
#include "creaturestats.hpp"
#include "movement.hpp"
#include "../mwworld/oblivioncombatdata.hpp"
#include "../mwworld/oblivionactorstats.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwphysics/actor.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/oblivionprofileservices.hpp"
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadweap.hpp>
#include "../mwworld/worldimp.hpp"
#include "../mwrender/animation.hpp"
#include <components/sceneutil/keyframe.hpp>
#include <components/esm/records.hpp>
#include <components/esm4/combatsettings.hpp>
#include <components/misc/strings/algorithm.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

namespace MWMechanics
{
    OblivionMeleeSequenceMetadata captureOblivionMeleeSequenceMetadata(
        MWRender::Animation& animation, std::string_view group, bool reaction)
    {
        const auto* source = animation.getControllerSequenceMetadata(group);
        if (!source || (source->mCycleType != 2 && (reaction || source->mCycleType != 0)))
            throw std::invalid_argument("native reaction requires clamp; outgoing sequence requires loop or clamp");
        std::vector<ESM4::MeleeTextKey> raw;
        raw.reserve(source->mTextKeys.size());
        for (const auto& [time, text] : source->mTextKeys) raw.push_back({time, text});
        const auto frames = ESM4::meleeBlendFrames(raw);
        const auto blend = std::bit_cast<std::int8_t>(frames);
        // Reviewed original General INI defaults. Custom original INI import
        // is separate: fAnimationDefaultBlend=.1 and fAnimationMult=1.
        const float retirement = blend == 0 ? .1f : static_cast<float>(double(blend) / 30.);
        OblivionMeleeSequenceMetadata result{group, source->mFrequency,
            source->mStartTime, source->mStopTime, source->mTimelineStart, source->mTimelineStop, {}, retirement};
        result.mBlendFrames = frames;
        result.mCycle = source->mCycleType == 0 ? ESM4::MeleeSequenceCycle::Loop : ESM4::MeleeSequenceCycle::Clamp;
        if (reaction)
        {
            const auto keys = ESM4::reactionKeyTimes(raw);
            if (keys.mMatchedCount != 2)
                throw std::invalid_argument("native reaction requires Start/End keys");
            result.mReactionKeys = keys.mTimes;
        }
        result.validate();
        return result;
    }

    OblivionMeleeAcquisitionSettings resolveOblivionMeleeAcquisitionSettings(
        std::span<const ESM4::GameSetting* const> settings)
    {
        const float cone = ESM4::buildCombatHitConeAngle(settings); // Validates the winning inventory.
        float slope = 48.f; // Original initializer9EA85F, storageB37330.
        for (const auto* setting : settings)
            if (Misc::StringUtils::ciEqual(setting->mEditorId, "fAICombatSlopeDifference"))
            {
                const auto* value = std::get_if<float>(&setting->mData);
                if (!value)
                    throw std::invalid_argument("incorrect native melee slope setting type");
                slope = *value;
            }
        if (!std::isfinite(slope) || slope < 0)
            throw std::invalid_argument("invalid native melee slope difference");
        return {cone, slope};
    }

    ESM4::MeleeDistanceActor oblivionMeleeBody(const MWPhysics::Actor& body,
        float combatScale, bool swimming)
    {
        const auto ptr = body.getPtr();
        const auto position = ptr.getRefData().getPosition().asVec3();
        const auto center = body.getCollisionObjectPosition();
        const auto extents = body.getHalfExtents();
        const auto originalCenter = body.getOriginalCollisionCenter();
        const auto originalExtents = body.getOriginalHalfExtents();
        return {{position.x(), position.y(), position.z()},
            static_cast<float>(double(center.z()) - extents.z() - position.z()),
            static_cast<float>(double(center.z()) + extents.z() - position.z()),
            static_cast<float>(double(originalCenter.y()) + originalExtents.y()),
            combatScale, true, swimming};
    }

    static std::optional<MWWorld::Ptr> queryOblivionMeleeContact(MWBase::World& world,
        std::optional<std::uint64_t> actionId, const MWWorld::Ptr& attacker,
        const MWWorld::Ptr& selectedTarget, float reach)
    {
        if (!std::isfinite(reach) || reach < 0)
            throw std::invalid_argument("invalid native melee acquisition reach");
        if (world.getGameProfile() != ESM::GameProfile::Oblivion || attacker.isEmpty())
            return std::nullopt;
        auto* nativeWorld = dynamic_cast<MWWorld::World*>(&world);
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        if (!service)
            return std::nullopt;
        const auto player = world.getPlayerPtr();
        const auto nativeActor = [&](const MWWorld::Ptr& ptr) {
            return !ptr.isEmpty() && (ptr == player || ptr.getType() == ESM::REC_NPC_4
                || ptr.getType() == ESM::REC_CREA4);
        };
        const auto actorKey = [&](const MWWorld::Ptr& ptr) {
            return ptr == player ? ESM::FormKey::dynamic("player", 1) : ptr.getCellRef().getFormKey();
        };
        if (!nativeActor(attacker))
            return std::nullopt;
        const auto source = actorKey(attacker);
        if (actionId && !service->isActionPending(*actionId, source))
            return std::nullopt;
        const auto* life = service->findActorLife(source);
        if (!life || life->mPhase != ESM4::ActorLifePhase::Alive)
            return std::nullopt;
        if (attacker != player)
            (void)service->getNonPlayerValue(attacker, 8); // Validate actual base/Ptr binding.
        const auto* physics = dynamic_cast<const MWPhysics::PhysicsSystem*>(world.getRayCasting());
        const auto* sourceBody = physics ? physics->getActor(attacker) : nullptr;
        if (!sourceBody || !attacker.isInCell() || !attacker.getRefData().isEnabled())
            return std::nullopt;
        const auto scale = [](const MWWorld::Ptr& ptr) {
            const float placed = ptr.getCellRef().getScale();
            osg::Vec3f value(placed, placed, placed);
            ptr.getClass().adjustScale(ptr, value, true);
            return value.z(); // Native NPC reach uses race height, not race weight.
        };
        const auto swimming = [&](const MWWorld::Ptr& ptr) {
            const auto* values = service->findActorValues(actorKey(ptr));
            // Original +2C0 returns zero outside High process. This fork's
            // admitted active collision actors are the High-process adapter.
            return values && values->mProcess == ESM4::ActorValueProcess::Active && world.isSwimming(ptr);
        };
        const auto sourceGeometry = oblivionMeleeBody(*sourceBody, scale(attacker), swimming(attacker));
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& record : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(record.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(record.mId));
        const auto acquisition = resolveOblivionMeleeAcquisitionSettings(settings);
        std::vector<MWWorld::Ptr> actors;
        if (!selectedTarget.isEmpty())
            actors.push_back(selectedTarget);
        else
            MWBase::Environment::get().getMechanicsManager()->getActorsInRange(
                attacker.getRefData().getPosition().asVec3(), std::numeric_limits<float>::max(), actors);
        std::vector<ESM4::MeleeContactCandidate> candidates;
        candidates.reserve(actors.size());
        for (const auto& target : actors)
        {
            ESM4::MeleeContactCandidate candidate{std::numeric_limits<float>::max(), 0, false, false};
            const auto* body = nativeActor(target) && target != attacker && target.isInCell()
                && target.getRefData().isEnabled() ? physics->getActor(target) : nullptr;
            if (body && target.getCell()->getCell()->getWorldSpace()
                == attacker.getCell()->getCell()->getWorldSpace())
            {
                const auto* targetLife = service->findActorLife(actorKey(target));
                candidate.mEligible = targetLife && (actionId ? targetLife->mPhase != ESM4::ActorLifePhase::Dead
                    : targetLife->mPhase == ESM4::ActorLifePhase::Alive);
                const auto geometry = oblivionMeleeBody(*body, scale(target), swimming(target));
                const auto& a = sourceGeometry.mPosition;
                const auto& b = geometry.mPosition;
                const float dx = static_cast<float>(double(b[0]) - a[0]);
                const float dy = static_cast<float>(double(b[1]) - a[1]);
                const float dz = static_cast<float>(double(b[2]) - a[2]);
                const float squared = static_cast<float>(double(dx) * dx + double(dy) * dy + double(dz) * dz);
                const float rawDistance = static_cast<float>(std::sqrt(double(squared)));
                candidate.mDistance = ESM4::meleeContactDistance(rawDistance, sourceGeometry,
                    geometry, !selectedTarget.isEmpty(), acquisition.mSlopeDifference);
                const auto facing = ESM4::combatHitCone(attacker.getRefData().getPosition().rot[2],
                    std::atan2(dx, dy), acquisition.mConeDegrees);
                candidate.mFacingDegrees = facing.mDegrees;
                candidate.mInsideCone = facing.mInside;
            }
            candidates.push_back(candidate);
        }
        const auto selected = ESM4::selectMeleeContact(candidates, reach,
            selectedTarget.isEmpty() ? std::nullopt : std::optional<std::size_t>{0});
        if (!selected || !world.getLOS(attacker, actors[*selected]))
            return MWWorld::Ptr{};
        return actors[*selected];
    }
    std::optional<MWWorld::Ptr> acquireOblivionMeleeContact(MWBase::World& world,
        std::uint64_t actionId, const MWWorld::Ptr& attacker,
        const MWWorld::Ptr& selectedTarget, float reach)
    {
        return queryOblivionMeleeContact(world, actionId, attacker, selectedTarget, reach);
    }

    bool canReachOblivionMeleeTarget(MWBase::World& world, const MWWorld::Ptr& attacker,
        const MWWorld::Ptr& target, float reach)
    {
        if (target.isEmpty())
            return false; // Never enumerate substitutes for a missing selected target.
        const auto result = queryOblivionMeleeContact(world, std::nullopt, attacker, target, reach);
        return result && *result == target;
    }

    bool updateOblivionStationaryMeleeAi(MWBase::World& world, const MWWorld::Ptr& actor, bool enabled)
    {
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* combat = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        auto* ai = nativeWorld ? nativeWorld->getOblivionAiService() : nullptr;
        if (!combat || !ai || actor.isEmpty()
            || (actor.getType() != ESM::REC_NPC_4 && actor.getType() != ESM::REC_CREA4))
            return false;
        const auto key = actor.getCellRef().getFormKey();
        auto& stats = actor.getClass().getCreatureStats(actor);
        // This branch owns C++ AI controls. Lua disableAI is handled by the
        // caller before invoking it; Lua-controlled input is not cleared here.
        stats.setAttackingOrSpell(false);
        const bool engaged = combat->isInCombat(key);
        const auto* values = combat->findActorValues(key);
        const auto* life = combat->findActorLife(key);
        if (!enabled || !engaged || !values || !life || life->mPhase != ESM4::ActorLifePhase::Alive
            || values->mProcess != ESM4::ActorValueProcess::Active || ai->isRestrained(actor)
            || ai->isRidingHorse(actor) || oblivionParalyzed(world, actor) || oblivionKnockedState(world, actor) != 0)
        {
            combat->clearMeleeAiIntent(key);
            return engaged;
        }
        if (const auto* package = ai->state(actor); package && (package->mActionReserved
                || package->mBoundary == ESM4::PhaseBoundary::WaitingForM16Action))
        {
            combat->clearMeleeAiIntent(key);
            return false; // Retain the existing package's reserved action authority.
        }
        if (const auto* melee = combat->findMeleeState(key); melee && melee->mStrike
            && melee->mStrike->mKind == ESM4::MeleeStrikeKind::Counter)
            return true; // The defensive controller owns its untargeted Attack-key acquisition.
        const auto policy = MWWorld::resolveOblivionCombatPolicy(world.getStore(), values->mBase,
            MWWorld::buildOblivionCombatDefaults(world.getStore()));
        const auto& style = policy.mStandard;
        constexpr auto stationaryFlags = static_cast<std::uint8_t>(ESM4::CombatStyleFlag::ChooseAttackChance)
            | static_cast<std::uint8_t>(ESM4::CombatStyleFlag::DisableFleeing);
        if (policy.mStyle.isNull() || style.mFlags != stationaryFlags || style.mAttackChance != 100
            || style.mBlockChance != 0 || style.mDodgeChance != 0 || style.mAcrobaticDodgeChance != 0
            || style.mPowerAttackChance != 0 || style.mIdle.mMinimum != 0 || style.mIdle.mMaximum != 0
            || style.mHold.mMinimum != 0 || style.mHold.mMaximum != 0
            || style.mAttackRecoilBonus != 0 || style.mAttackUnconsciousBonus != 0 || style.mAttackUnarmedBonus != 0
            || style.mPowerAttackRecoilBonus != 0 || style.mPowerAttackUnconsciousBonus != 0
            || style.mDoNotAcquire != true)
        {
            combat->clearMeleeAiIntent(key);
            throw std::runtime_error("native stationary melee requires the explicit S4 deterministic CSTY profile");
        }
        const auto opponents = combat->combatOpponents(key);
        if (opponents.size() != 1)
        {
            combat->clearMeleeAiIntent(key);
            throw std::runtime_error("native stationary melee requires one explicit combat opponent");
        }
        const auto target = opponents.front() == ESM::FormKey::dynamic("player", 1)
            ? world.getPlayerPtr() : ai->resolveReference(opponents.front());
        const auto* targetLife = combat->findActorLife(opponents.front());
        if (target.isEmpty() || !target.isInCell() || !target.getRefData().isEnabled()
            || !targetLife || targetLife->mPhase != ESM4::ActorLifePhase::Alive)
        {
            combat->clearMeleeAiIntent(key);
            return true;
        }
        const auto* physics = dynamic_cast<const MWPhysics::PhysicsSystem*>(world.getRayCasting());
        if (!physics || !physics->getActor(actor) || !physics->getActor(target))
        {
            combat->clearMeleeAiIntent(key);
            return true; // Lost collision residence cannot turn into a substituted hit or cost.
        }
        combat->setMeleeAiIntent(key, {opponents.front(), policy.mStyle}, world.getStore());
        auto& movement = actor.getClass().getMovementSettings(actor);
        std::fill(std::begin(movement.mPosition), std::end(movement.mPosition), 0.f);
        std::fill(std::begin(movement.mRotation), std::end(movement.mRotation), 0.f);
        stats.setDrawState(DrawState::Weapon);
        const auto* melee = combat->findMeleeState(key);
        if (melee && melee->mStrike)
            return true; // Release input while the actual controller owns playback.
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& setting : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(setting.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(setting.mId));
        const auto reachSettings = ESM4::buildMeleeReachSettings(settings);
        const float placed = actor.getCellRef().getScale();
        osg::Vec3f scale(placed, placed, placed);
        actor.getClass().adjustScale(actor, scale, true);
        const bool creature = actor.getType() == ESM::REC_CREA4;
        const auto naturalReach = creature ? actor.get<ESM4::Creature>()->mBase->mAttackReach : std::nullopt;
        if (creature && !naturalReach)
            throw std::invalid_argument("native stationary Creature lacks RNAM reach");
        float reach = creature ? ESM4::creatureMeleeReach(*naturalReach,
            actor.get<ESM4::Creature>()->mBase->mData.creatureType, scale.z(), reachSettings)
            : ESM4::unarmedMeleeReach(scale.z(), reachSettings);
        auto& inventory = actor.getClass().getInventoryStore(actor);
        if (const auto equipped = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
            equipped != inventory.end())
        {
            if (creature)
            {
                combat->clearMeleeAiIntent(key);
                throw std::runtime_error("native stationary Creature weapon contact is not admitted");
            }
            const auto id = MWWorld::OblivionProfileServices::nativeItemId(world.getStore(), equipped->getCellRef().getRefId());
            const auto* form = id.getIf<ESM::FormId>();
            const auto* weapon = form ? world.getStore().get<ESM4::Weapon>().search(*form) : nullptr;
            if (!weapon || weapon->mData.type > 3)
            {
                combat->clearMeleeAiIntent(key);
                throw std::runtime_error("native stationary melee requires unarmed or ordinary melee WEAP equipment");
            }
            reach = ESM4::weaponMeleeReach(weapon->mData.reach, scale.z(), reachSettings);
        }
        stats.setAttackingOrSpell(canReachOblivionMeleeTarget(world, actor, target, reach));
        return true;
    }

    std::optional<OblivionOrdinaryContactResult> commitOblivionMeleeContact(
        MWBase::World& world, std::uint64_t actionId,
        const MWWorld::Ptr& attacker, const MWWorld::Ptr& selectedTarget,
        float reach, float weaponWeight, float normalizedDifficulty, bool sneaking)
    {
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        if (!service || attacker.isEmpty())
            return std::nullopt;
        const auto actor = attacker == world.getPlayerPtr() ? ESM::FormKey::dynamic("player", 1)
            : attacker.getCellRef().getFormKey();
        if (!service->isMeleeContactPending(actionId, actor))
            return std::nullopt;
        const auto kind = service->findMeleeState(actor)->mStrike->mKind;
        const bool creature = attacker.getType() == ESM::REC_CREA4;
        if (creature && !service->findMeleeState(actor)->mStrike->mWeaponBase.isNull())
            return std::nullopt; // Armed Creature multipliers/wear remain a separate caller.
        // Directional unlocked mastery consequences must join this transaction
        // before those contacts are admitted. Standing power has no proc hook.
        if (kind > ESM4::MeleeStrikeKind::StandingPower)
            return std::nullopt;
        const bool power = kind == ESM4::MeleeStrikeKind::StandingPower;
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& record : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(record.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(record.mId));
        const float cost = ESM4::attackFatigueCost(weaponWeight, power && !creature,
            ESM4::buildAttackFatigueSettings(settings));
        const auto contact = acquireOblivionMeleeContact(world, actionId, attacker, selectedTarget, reach);
        if (!contact)
            return std::nullopt;
        OblivionOrdinaryContactResult result{*contact, {0, 0}};
        std::int32_t soundWeaponType = -1;
        std::int32_t hitArmorClass = -1;
        float preArmorDamage = 0;
        bool blockAccepted = false;
        bool weaponBlockSound = false;
        OblivionPhysicalContactDeltas deltas{-cost, 0, 0, 0};
        if (!contact->isEmpty())
        {
            const auto* state = service->findMeleeState(actor);
            if (!state || !state->mStrike)
                return std::nullopt;
            if (state->mStrike->mWeaponBase.isNull())
            {
                const auto damage = resolveOblivionUnarmedMeleeContact(world, attacker,
                    *contact, normalizedDifficulty, sneaking, kind);
                if (!damage)
                    return std::nullopt;
                result.mDamage = {damage->mHealth, damage->mFatigue};
                result.mBlockFatigueDebit = damage->mBlockFatigueDebit;
                result.mBlockAbsorbedFraction = damage->mBlockAbsorbedFraction;
                blockAccepted = damage->mBlockAccepted;
                weaponBlockSound = damage->mWeaponBlockSound;
                result.mHitSounds = damage->mBlockSounds;
                hitArmorClass = damage->mHitArmorClass;
                preArmorDamage = damage->mPreArmorDamage;
                deltas.mConditionChanges = damage->mVictimConditionChanges;
                deltas.mRandomTransition = damage->mRandomTransition;
                result.mArmorConditionWrites = static_cast<unsigned>(std::count_if(
                    damage->mVictimConditionChanges.begin(), damage->mVictimConditionChanges.end(),
                    [](const auto& change) { return change.mItem.getType() == ESM::REC_ARMO; }));
                result.mRandomDraws = damage->mRandomTransition ? damage->mRandomTransition->mDraws : 0;
            }
            else
            {
                auto& inventory = attacker.getClass().getInventoryStore(attacker);
                const auto equipped = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
                if (equipped == inventory.end() || equipped->getType() != ESM::REC_WEAP)
                    return std::nullopt;
                const auto item = *equipped;
                const auto id = MWWorld::OblivionProfileServices::nativeItemId(
                    world.getStore(), item.getCellRef().getRefId());
                const auto* form = id.getIf<ESM::FormId>();
                if (!form || ESM::FormKeyResolver(world.getContentFiles()).toFormKey(*form)
                    != state->mStrike->mWeaponBase)
                    return std::nullopt;
                const auto* weapon = world.getStore().get<ESM4::Weapon>().search(*form);
                if (!weapon || weapon->mData.type > 3)
                    return std::nullopt;
                soundWeaponType = static_cast<std::int32_t>(weapon->mData.type);
                auto condition = captureOblivionPhysicalConditionChange(attacker, item, 0);
                const auto damage = resolveOblivionWeaponMeleeContact(world, attacker,
                    *contact, item, normalizedDifficulty, sneaking, kind);
                if (!damage)
                    return std::nullopt;
                result.mDamage = {damage->mHealth, 0};
                result.mBlockFatigueDebit = damage->mBlockFatigueDebit;
                result.mBlockAbsorbedFraction = damage->mBlockAbsorbedFraction;
                blockAccepted = damage->mBlockAccepted;
                weaponBlockSound = damage->mWeaponBlockSound;
                result.mHitSounds = damage->mBlockSounds;
                hitArmorClass = damage->mHitArmorClass;
                result.mWeaponConditionAfterWear = damage->mConditionAfterWear;
                result.mArmorConditionWrites = static_cast<unsigned>(std::count_if(
                    damage->mVictimConditionChanges.begin(), damage->mVictimConditionChanges.end(),
                    [](const auto& change) { return change.mItem.getType() == ESM::REC_ARMO; }));
                result.mRandomDraws = damage->mRandomTransition ? damage->mRandomTransition->mDraws : 0;
                if (damage->mConditionAfterWear)
                {
                    condition.mCondition = *damage->mConditionAfterWear;
                    condition.mBreakDisposition = damage->mBreakDisposition;
                    deltas.mConditionChanges.push_back(std::move(condition));
                }
                deltas.mConditionChanges.insert(deltas.mConditionChanges.end(),
                    damage->mVictimConditionChanges.begin(), damage->mVictimConditionChanges.end());
                deltas.mRandomTransition = damage->mRandomTransition;
                preArmorDamage = damage->mPreArmorDamage;
            }
        }
        if (weaponBlockSound)
        {
            // Original6B00F6 computes the midpoint from native world positions.
            // Independent position voices preserve two identical block layers;
            // actor-bound playback replaces the first copy of the same SOUN.
            const auto& from = attacker.getRefData().getPosition();
            const auto& to = contact->getRefData().getPosition();
            std::array<float, 3> position;
            for (std::size_t i = 0; i < position.size(); ++i)
                position[i] = static_cast<float>((double(from.pos[i]) + to.pos[i]) * .5);
            result.mHitSoundPosition = position;
        }
        deltas.mVictimHealth = -result.mDamage.mHealth;
        const bool removedBlockingShield = blockAccepted && !result.mHitSounds
            && std::any_of(deltas.mConditionChanges.begin(), deltas.mConditionChanges.end(), [&](const auto& change) {
                return change.mOwner == *contact && change.mItem.getType() == ESM::REC_ARMO
                    && change.mBreakDisposition != OblivionWeaponBreakDisposition::None;
            });
        if (!contact->isEmpty() && (*contact == world.getPlayerPtr() || contact->getType() == ESM::REC_NPC_4
                || contact->getType() == ESM::REC_CREA4)
            && (!blockAccepted || removedBlockingShield))
        {
            // Original5FFB05 re-reads the shield after blocking wear. A removed shield
            // uses general hit layers even when the contact was blocked.
            // Original5FFCE5 uses the armor selected by this contact's wear
            // path;5FFD61 uses no material even if other armor is equipped.
            // Model initialization projects Creature Actor byte104 as
            // canonical0/1; the TESCreature base type is not this sound input.
            result.mHitSounds = ESM4::nativeMeleeHitSounds({soundWeaponType, hitArmorClass},
                contact->getType() == ESM::REC_CREA4
                    ? std::optional<std::uint8_t>{contact->getRefData().getOblivionCanSpendFatigue() ? 1u : 0u}
                    : std::nullopt);
        }
        deltas.mVictimFatigue = -result.mDamage.mFatigue;
        deltas.mVictimBlockFatigue = -result.mBlockFatigueDebit;
        if (!contact->isEmpty() && (contact->getType() == ESM::REC_NPC_4 || contact->getType() == ESM::REC_CREA4)
            && result.mBlockAbsorbedFraction == 0 && oblivionKnockedState(world, *contact) == 0
            && service->getNonPlayerIntegerValue(*contact, 4) > 0)
        {
            const auto* physics = dynamic_cast<const MWPhysics::PhysicsSystem*>(world.getRayCasting());
            const auto* actorPhysics = physics ? physics->getActor(*contact) : nullptr;
            if (actorPhysics && actorPhysics->getCollisionMode())
            {
                const auto& from = attacker.getRefData().getPosition();
                const auto& to = contact->getRefData().getPosition();
                std::array<float, 3> delta;
                for (std::size_t i = 0; i < delta.size(); ++i)
                    delta[i] = static_cast<float>(double(to.pos[i]) - from.pos[i]);
                const auto targetKey = contact->getCellRef().getFormKey();
                deltas.mKnockback = OblivionKnockbackContact{delta, ESM4::combatBaseValue(preArmorDamage),
                    service->getNonPlayerBaseValue(targetKey, 10, world.getStore()),
                    ESM4::buildKnockbackSettings(settings), ESM4::buildPhysicalCombatSettings(settings)};
            }
        }
        const bool blockHit = blockAccepted && !contact->isEmpty()
            && (*contact == world.getPlayerPtr() || contact->getType() == ESM::REC_NPC_4);
        // The verified nonprojectile, positive-absorption branch requests
        // Recoil on an intact Player or NPC attacker too. Projected broken
        // equipment and zero-absorption unarmed reactions
        // need their separate resource/ownership paths.
        const bool attackerRecoil = blockHit && result.mBlockAbsorbedFraction > 0
            && (attacker == world.getPlayerPtr() || attacker.getType() == ESM::REC_NPC_4)
            && std::none_of(deltas.mConditionChanges.begin(), deltas.mConditionChanges.end(), [&](const auto& change) {
                return change.mOwner == attacker
                    && change.mBreakDisposition != OblivionWeaponBreakDisposition::None;
            });
        auto prepared = nativeWorld->prepareOblivionPhysicalContact(
            actionId, attacker, *contact, deltas, blockHit, attackerRecoil);
        if (!prepared || !prepared->commit())
            return std::nullopt;
        result.mRandomDraws = prepared->randomDraws();
        return result;
    }

    std::optional<OblivionOrdinaryContactResult> commitOblivionOrdinaryMeleeContact(
        MWBase::World& world, std::uint64_t actionId, const MWWorld::Ptr& attacker,
        const MWWorld::Ptr& selectedTarget, float reach, float weaponWeight,
        float normalizedDifficulty, bool sneaking)
    {
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        if (!service || attacker.isEmpty()) return std::nullopt;
        const auto actor = attacker == world.getPlayerPtr() ? ESM::FormKey::dynamic("player", 1)
            : attacker.getCellRef().getFormKey();
        if (!service->isOrdinaryMeleeContactPending(actionId, actor)) return std::nullopt;
        return commitOblivionMeleeContact(world, actionId, attacker, selectedTarget,
            reach, weaponWeight, normalizedDifficulty, sneaking);
    }

    std::int16_t oblivionProcessAction(MWBase::World& world, const MWWorld::Ptr& actor)
    {
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        if (!service || actor.isEmpty())
            throw std::invalid_argument("native action query requires a native actor");
        const bool player = actor == world.getPlayerPtr();
        if (!player && actor.getType() != ESM::REC_NPC_4 && actor.getType() != ESM::REC_CREA4)
            throw std::invalid_argument("native action query requires a native actor");
        // Resolve and validate the actual actor/base binding without importing
        // a shared animation flag or modifying its actor-value projection.
        if (player)
            service->getPlayerValue(8);
        else
            service->getNonPlayerValue(actor, 8);
        return service->getProcessAction(player ? ESM::FormKey::dynamic("player", 1)
            : actor.getCellRef().getFormKey());
    }

    bool oblivionBlockingPosture(MWBase::World& world, const MWWorld::Ptr& actor)
    {
        return ESM4::nativeBlockingPosture(oblivionProcessAction(world, actor));
    }

    bool oblivionParalyzed(MWBase::World& world, const MWWorld::Ptr& actor)
    {
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        if (!service || actor.isEmpty())
            throw std::invalid_argument("native paralysis query requires a native actor");
        const bool player = actor == world.getPlayerPtr();
        if (!player && actor.getType() != ESM::REC_NPC_4 && actor.getType() != ESM::REC_CREA4)
            throw std::invalid_argument("native paralysis query requires a native actor");
        if (player)
            service->getPlayerValue(8);
        else
            service->getNonPlayerValue(actor, 8);
        return service->getProcessParalysis(player ? ESM::FormKey::dynamic("player", 1)
            : actor.getCellRef().getFormKey()) != 0;
    }

    std::int8_t oblivionKnockedState(MWBase::World& world, const MWWorld::Ptr& actor)
    {
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        if (!service || actor.isEmpty())
            throw std::invalid_argument("native knocked query requires a native actor");
        const bool player = actor == world.getPlayerPtr();
        if (!player && actor.getType() != ESM::REC_NPC_4 && actor.getType() != ESM::REC_CREA4)
            throw std::invalid_argument("native knocked query requires a native actor");
        // Resolve and validate the actual actor/base binding without importing
        // a shared animation flag or modifying its actor-value projection.
        if (player)
            service->getPlayerValue(8);
        else
            service->getNonPlayerValue(actor, 8);
        return service->getProcessKnockedState(player ? ESM::FormKey::dynamic("player", 1)
            : actor.getCellRef().getFormKey());
    }

    ESM4::HandToHandDamage oblivionHandToHandContactDamage(MWBase::World& world,
        const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim)
    {
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        if (!service || attacker.isEmpty())
            throw std::invalid_argument("native hand contact requires a native attacker");
        const bool player = attacker == world.getPlayerPtr();
        if (!player && attacker.getType() != ESM::REC_NPC_4)
            throw std::invalid_argument("native hand contact requires a Player or NPC attacker");
        const auto integer = [&](std::uint8_t index) {
            return player ? service->getPlayerIntegerValue(index)
                : service->getNonPlayerIntegerValue(attacker, index);
        };
        const auto key = player ? ESM::FormKey::dynamic("player", 1)
            : attacker.getCellRef().getFormKey();
        // Original hand caller reads +284 integer AVs and the separate floored
        // base Fatigue query. Armor/weapon float-query conversion differs.
        const ESM4::HandToHandContactInput input{integer(17), integer(7), integer(0),
            player ? service->getPlayerValue(10) : service->getNonPlayerValue(attacker, 10),
            player ? service->getPlayerBaseValue(10)
                : service->getNonPlayerBaseValue(key, 10, world.getStore()),
            oblivionKnockedState(world, victim)};
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& record : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(record.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(record.mId));
        return ESM4::handToHandContactDamage(input, ESM4::buildHandToHandSettings(settings),
            ESM4::buildPhysicalCombatSettings(settings));
    }

    ESM4::HandToHandDamage oblivionCreatureNaturalContactDamage(
        MWBase::World& world, const MWWorld::Ptr& attacker)
    {
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        if (!service || attacker.isEmpty() || attacker.getType() != ESM::REC_CREA4)
            throw std::invalid_argument("native natural contact requires a Creature attacker");
        // Resolve the actual binding before reading form inputs. The native
        // TESCreature damage getter recalculates PCLevelOffset scaling on each
        // query; the construction-time shared damage projection is insufficient.
        const float current = service->getNonPlayerValue(attacker, 10);
        const auto key = attacker.getCellRef().getFormKey();
        const auto& values = *service->findActorValues(key);
        const auto* record = world.getStore().search<ESM4::Creature>(values.mBase);
        if (!record)
            throw std::invalid_argument("native natural contact lacks its winning Creature form");
        std::optional<std::uint16_t> playerLevel;
        if (record->mBaseConfig.tes4.flags & ESM4::Creature::TES4_PCLevelOffset)
        {
            const auto player = world.getPlayerPtr();
            if (player.isEmpty())
                throw std::invalid_argument("scaled natural contact requires the native Player");
            const int level = player.getClass().getCreatureStats(player).getLevel();
            if (level < 1 || level > std::numeric_limits<std::uint16_t>::max())
                throw std::invalid_argument("scaled natural contact has invalid Player level");
            playerLevel = static_cast<std::uint16_t>(level);
        }
        const auto base = MWWorld::resolveOblivionActorBaseStats(world.getStore(), values.mBase, playerLevel);
        if (!base.mNaturalDamage)
            throw std::invalid_argument("native Creature lacks its damage word");
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& setting : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(setting.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(setting.mId));
        const float ratio = ESM4::combatFatigueRatio(current,
            service->getNonPlayerBaseValue(key, 10, world.getStore()));
        const auto damage = ESM4::creatureNaturalDamage(*base.mNaturalDamage, ratio,
            ESM4::buildPhysicalCombatSettings(settings));
        return {static_cast<float>(damage), 0};
    }

    float oblivionNormalizedDifficulty(int difficultySetting)
    {
        return static_cast<float>(double(std::clamp(difficultySetting, -100, 100)) / 100.0);
    }

    namespace
    {
        bool creatureHasSpells(const MWWorld::Ptr& actor)
        {
            if (actor.isEmpty() || actor.getType() != ESM::REC_CREA4)
                return false;
            const auto& spells = actor.get<ESM4::Creature>()->mBase->mSpell;
            return std::any_of(spells.begin(), spells.end(), [](ESM::FormId spell) {
                return spell != ESM::FormId{};
            });
        }

        bool creatureUsesDefaultWeaponHitSound(const MWWorld::ESMStore& store, const MWWorld::Ptr& actor)
        {
            // Original51CEC0 follows CSCR until flag100 selects an own sound
            // array. All installed master CREA groups lack Weapon9 entries.
            // A custom group needs the original per-candidate RNG transition;
            // retain an explicit unsupported boundary until that joins contact.
            constexpr std::uint32_t ownSounds = 0x100;
            const auto* record = actor.get<ESM4::Creature>()->mBase;
            std::set<ESM::FormKey> seen;
            while (!(record->mBaseConfig.tes4.flags & ownSounds))
            {
                if (!record->mSounds.empty())
                    return false; // Mixed local/inherited union load precedence is unreviewed.
                if (record->mSoundBaseKey.isNull())
                    return true;
                if (!seen.insert(record->mSoundBaseKey).second)
                    throw std::invalid_argument("cyclic native Creature sound inheritance");
                record = store.search<ESM4::Creature>(record->mSoundBaseKey);
                if (!record)
                    throw std::invalid_argument("missing native Creature sound inheritance base");
            }
            if (!record->mSoundBaseKey.isNull())
                return false;
            return std::none_of(record->mSounds.begin(), record->mSounds.end(), [](const auto& sound) {
                return sound.mType == ESM4::Creature::SoundType::Weapon;
            });
        }

        float contactPowerMultiplier(MWBase::World& world, OblivionCombatService& service,
            const MWWorld::Ptr& attacker, std::uint8_t skill, ESM4::MeleeStrikeKind kind,
            std::span<const ESM4::GameSetting* const> settings)
        {
            if (kind < ESM4::MeleeStrikeKind::StandingPower) return 1.f;
            if (kind > ESM4::MeleeStrikeKind::RightPower)
                throw std::invalid_argument("invalid native power contact kind");
            const auto current = attacker == world.getPlayerPtr() ? service.getPlayerIntegerValue(skill)
                : service.getNonPlayerIntegerValue(attacker, skill);
            const auto direction = static_cast<ESM4::PowerAttackDirection>(
                static_cast<unsigned>(kind) - static_cast<unsigned>(ESM4::MeleeStrikeKind::StandingPower));
            return ESM4::powerAttackMultiplier(current, direction, ESM4::buildPowerAttackSettings(settings),
                ESM4::buildCombatMasterySettings(settings));
        }

        struct PreparedArmorWear
        {
            std::vector<OblivionPhysicalConditionChange> mConditions;
            std::optional<OblivionCombatRandomTransition> mRandom;
            std::int32_t mHitArmorClass = -1;
        };

        std::optional<PreparedArmorWear> prepareOrdinaryArmorWear(MWBase::World& world,
            OblivionCombatService& service, const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim, float incoming,
            float absorbedFraction, std::span<const ESM4::GameSetting* const> settings,
            std::span<const OblivionPhysicalConditionChange> prior = {}, std::optional<float> adjustedWear = {});

        struct PreparedBlock
        {
            float mRemaining;
            float mFraction = 0;
            float mDebit = 0;
            bool mAccepted = false;
            bool mUnarmed = true;
            bool mWeaponBlockSound = false;
            std::optional<std::array<std::string_view, 3>> mSounds{};
            std::vector<OblivionPhysicalConditionChange> mConditions{};
        };

        OblivionWeaponBreakDisposition weaponBreakDisposition(MWBase::World& world,
            const OblivionCombatService& service, const MWWorld::Ptr& actor, const ESM4::Weapon& weapon)
        {
            const bool player = actor == world.getPlayerPtr();
            const auto key = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
            const auto* values = service.findActorValues(key);
            if (!values) throw std::logic_error("native weapon break has no actor authority");
            // Original5F39D3: only an active, weapon-out process requests an
            // equipment consequence. Undrawn/low-process wear still commits.
            if (values->mProcess != ESM4::ActorValueProcess::Active
                || actor.getClass().getCreatureStats(actor).getDrawState() != DrawState::Weapon)
                return OblivionWeaponBreakDisposition::None;
            return player || (weapon.mFlags & ESM4::Rec_Persistent)
                ? OblivionWeaponBreakDisposition::Unequip : OblivionWeaponBreakDisposition::Drop;
        }

        OblivionWeaponBreakDisposition shieldBreakDisposition(MWBase::World& world,
            const OblivionCombatService& service, const MWWorld::Ptr& actor, const ESM4::Armor& shield)
        {
            const bool player = actor == world.getPlayerPtr();
            const auto key = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
            const auto* values = service.findActorValues(key);
            if (!values) throw std::logic_error("native shield break has no actor authority");
            // Original5F3A10: active worn slot13, independent of weapon-out.
            if (values->mProcess != ESM4::ActorValueProcess::Active)
                return OblivionWeaponBreakDisposition::None;
            return player || (shield.mFlags & ESM4::Rec_Persistent)
                ? OblivionWeaponBreakDisposition::Unequip : OblivionWeaponBreakDisposition::Drop;
        }

        std::optional<PreparedBlock> prepareOrdinaryBlock(MWBase::World& world,
            OblivionCombatService& service, const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim,
            float remaining, std::int32_t attackerWeaponType, std::span<const ESM4::GameSetting* const> settings)
        {
            PreparedBlock result{remaining};
            if (!oblivionBlockingPosture(world, victim) || oblivionParalyzed(world, victim))
                return result;
            if (victim.getType() == ESM::REC_CREA4)
                return std::nullopt; // Creature defensive posture/equipment policy needs its native caller.
            const auto& from = attacker.getRefData().getPosition();
            const auto& to = victim.getRefData().getPosition();
            const float dx = static_cast<float>(double(from.pos[0]) - to.pos[0]);
            const float dy = static_cast<float>(double(from.pos[1]) - to.pos[1]);
            if (!ESM4::combatHitCone(to.rot[2], std::atan2(dx, dy),
                    ESM4::buildCombatHitConeAngle(settings)).mInside)
                return result;
            auto& inventory = victim.getClass().getInventoryStore(victim);
            MWWorld::Ptr item;
            const ESM4::Armor* shield = nullptr;
            const ESM4::Weapon* weapon = nullptr;
            if (auto left = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedLeft);
                left != inventory.end() && left->getType() == ESM::REC_ARMO)
            {
                const auto id = MWWorld::OblivionProfileServices::nativeItemId(world.getStore(), left->getCellRef().getRefId());
                const auto* form = id.getIf<ESM::FormId>();
                shield = form ? world.getStore().get<ESM4::Armor>().search(*form) : nullptr;
                if (!shield || !(shield->mArmorFlags & ESM4::Armor::TES4_Shield))
                    throw std::invalid_argument("native blocking shield has no winning shield definition");
                item = *left;
            }
            else if (auto right = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
                right != inventory.end() && right->getType() == ESM::REC_WEAP)
            {
                const auto id = MWWorld::OblivionProfileServices::nativeItemId(world.getStore(), right->getCellRef().getRefId());
                const auto* form = id.getIf<ESM::FormId>();
                weapon = form ? world.getStore().get<ESM4::Weapon>().search(*form) : nullptr;
                if (!weapon || weapon->mData.type > 3)
                    return std::nullopt;
                item = *right;
            }
            const auto equipment = shield ? ESM4::BlockEquipment::Shield
                : weapon ? ESM4::BlockEquipment::Weapon : ESM4::BlockEquipment::Unarmed;
            const auto disposition = ESM4::blockContactDisposition(
                {true, false, false, true, equipment, attackerWeaponType >= 0, false});
            if (disposition == ESM4::BlockContactDisposition::ReactionOnly)
                return std::nullopt; // Zero-absorption unarmed mastery/recoil remains a separate branch.
            const bool player = victim == world.getPlayerPtr();
            const auto key = player ? ESM::FormKey::dynamic("player", 1) : victim.getCellRef().getFormKey();
            const auto integer = [&](std::uint8_t av) { return player ? service.getPlayerIntegerValue(av)
                : service.getNonPlayerIntegerValue(victim, av); };
            const auto base = [&](std::uint8_t av) { return player ? service.getPlayerBaseValue(av)
                : service.getNonPlayerBaseValue(key, av, world.getStore()); };
            // Expert/Master defensive procs need their complete recoil/drop
            // transaction. Never publish an ordinary hit while omitting them.
            if (ESM4::combatMastery(base(item.isEmpty() ? 17 : 15),
                    ESM4::buildCombatMasterySettings(settings)) >= ESM4::CombatMastery::Expert)
                return std::nullopt;
            double itemCondition = 0;
            if (!item.isEmpty())
            {
                const auto& ref = item.getCellRef();
                itemCondition = ref.getNativeItemCondition() ? double(*ref.getNativeItemCondition())
                    : ref.getCharge() < 0 ? double(shield ? shield->mData.health : weapon->mData.health)
                    : double(ref.getCharge()) + ref.getChargeIntRemainder();
                if (!std::isfinite(itemCondition) || itemCondition < 0)
                    throw std::invalid_argument("invalid native blocking item condition");
                // A worn zero-condition item can still block. Native5F5C69
                // calls its wear helper only below Journeyman; positive wear
                // then reports the break, while zero wear leaves it equipped.
            }
            const float fatigue = player ? service.getPlayerValue(10) : service.getNonPlayerValue(victim, 10);
            result.mAccepted = true;
            result.mUnarmed = item.isEmpty();
            if (shield)
                result.mSounds = ESM4::nativeNpcMeleeHitSounds({attackerWeaponType, -1,
                    (shield->mGeneralFlags & ESM4::Armor::TES4_HeavyArmor) ? 1 : 0});
            else if (weapon)
            {
                result.mWeaponBlockSound = true;
                const auto layers = ESM4::nativeMeleeBlockSounds(attackerWeaponType,
                    static_cast<std::int32_t>(weapon->mData.type));
                result.mSounds = std::array<std::string_view, 3>{layers[0], layers[1], {}};
            }
            result.mFraction = ESM4::blockFraction({integer(15), integer(7),
                ESM4::combatFatigueRatio(fatigue, base(10)), equipment},
                ESM4::buildBlockSettings(settings), ESM4::buildPhysicalCombatSettings(settings));
            const auto costs = ESM4::blockContactCosts(base(15), integer(15), remaining,
                result.mFraction, !item.isEmpty(), ESM4::buildBlockCostSettings(settings),
                ESM4::buildCombatMasterySettings(settings));
            result.mDebit = costs.mFatigueDebit;
            result.mRemaining = ESM4::blockedHealthDamage(remaining, result.mFraction);
            if (costs.mBlockingItemWear > 0 && !(player && world.getGodModeState()))
            {
                const double current = itemCondition;
                std::optional<float> condition;
                if (shield)
                {
                    const bool heavy = shield->mGeneralFlags & ESM4::Armor::TES4_HeavyArmor;
                    condition = ESM4::nativeArmorConditionAfterWear(current, costs.mBlockingItemWear,
                        base(heavy ? 18 : 27), heavy ? ESM4::ArmorWeight::Heavy : ESM4::ArmorWeight::Light,
                        ESM4::buildArmorWearMasterySettings(settings), ESM4::buildCombatMasterySettings(settings));
                }
                else
                    condition = ESM4::nativeConditionAfterWear(current, costs.mBlockingItemWear);
                if (condition)
                {
                    auto change = captureOblivionPhysicalConditionChange(victim, item, *condition);
                    if (*condition == 0)
                    {
                        change.mBreakDisposition = shield ? shieldBreakDisposition(world, service, victim, *shield)
                            : weaponBreakDisposition(world, service, victim, *weapon);
                        if (change.mBreakDisposition == OblivionWeaponBreakDisposition::None)
                            return std::nullopt; // Undrawn broken-blocking-item state is not admitted here.
                        // Original5FF914 re-reads the carried weapon after
                        // blocking wear. Both unequip and drop leave it empty.
                        if (shield)
                            result.mSounds.reset(); // Final material is selected after projected shield removal.
                        else
                        {
                            const auto layers = ESM4::nativeMeleeBlockSounds(attackerWeaponType, -1);
                            result.mSounds = std::array<std::string_view, 3>{layers[0], layers[1], {}};
                        }
                    }
                    result.mConditions.push_back(std::move(change));
                }
            }
            return result;
        }

    }

    std::optional<OblivionUnarmedContactDamage> resolveOblivionUnarmedMeleeContact(
        MWBase::World& world, const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim,
        float normalizedDifficulty, bool sneaking, ESM4::MeleeStrikeKind kind)
    {
        if (!std::isfinite(normalizedDifficulty) || normalizedDifficulty < -1 || normalizedDifficulty > 1)
            throw std::invalid_argument("native unarmed contact difficulty outside normalized slider domain");
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        if (!service)
            return std::nullopt;
        const auto player = world.getPlayerPtr();
        const auto eligible = [&](const MWWorld::Ptr& ptr) {
            return !ptr.isEmpty() && (ptr == player || ptr.getType() == ESM::REC_NPC_4);
        };
        const bool creature = !attacker.isEmpty() && attacker.getType() == ESM::REC_CREA4;
        const bool creatureVictim = !victim.isEmpty() && victim.getType() == ESM::REC_CREA4;
        if ((!creature && !eligible(attacker)) || (!creatureVictim && !eligible(victim)))
            return std::nullopt;
        const auto integer = [&](const MWWorld::Ptr& ptr, std::uint8_t av) {
            return ptr == player ? service->getPlayerIntegerValue(av)
                : service->getNonPlayerIntegerValue(ptr, av);
        };
        // These branches require sneak/mastery, block or gear-wear policy.
        // Do not substitute a generic TES3 hit or silently omit their effects.
        if (sneaking || integer(victim, 65) != 0)
            return std::nullopt;
        if (kind > ESM4::MeleeStrikeKind::StandingPower) return std::nullopt;
        auto incoming = creature ? oblivionCreatureNaturalContactDamage(world, attacker)
            : oblivionHandToHandContactDamage(world, attacker, victim);
        if (creatureHasSpells(attacker) || creatureHasSpells(victim))
            return std::nullopt; // Creature spell/contact-effect resolution must join this transaction.
        if (creature && !creatureUsesDefaultWeaponHitSound(world.getStore(), attacker))
            return std::nullopt;
        if (creature && incoming.mHealth <= 0)
            return std::nullopt; // The native nonpositive contact consequences remain unreviewed.
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& record : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(record.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(record.mId));
        const float multiplier = creature ? 1.f
            : contactPowerMultiplier(world, *service, attacker, 17, kind, settings);
        // Native5FF46C stores Health and Fatigue separately after the complete
        // hand calculation, including knocked-state Fatigue suppression.
        incoming.mHealth = static_cast<float>(double(incoming.mHealth) * multiplier);
        incoming.mFatigue = static_cast<float>(double(incoming.mFatigue) * multiplier);
        const auto armor = ESM4::mitigateArmor(incoming.mHealth, oblivionArmorRating(world, victim),
            ESM4::buildArmorRatingSettings(settings).mSkillMaximum, false);
        const auto block = prepareOrdinaryBlock(world, *service, attacker, victim, armor.mHealthDamage, -1, settings);
        if (!block)
            return std::nullopt;
        const auto role = victim == player ? ESM4::PlayerDamageRole::Victim
            : attacker == player ? ESM4::PlayerDamageRole::Attacker : ESM4::PlayerDamageRole::Unaffected;
        const auto damage = ESM4::physicalContactDamage({incoming.mHealth, incoming.mFatigue}, block->mRemaining,
            normalizedDifficulty, ESM4::buildDifficultyDamageMultiplier(settings), role);
        PreparedArmorWear wear{block->mConditions, {}};
        // Original5FF77A computes armor wear only when incoming hand Fatigue
        // is nonpositive. Knocked victims suppress that Fatigue writer while
        // retaining Health damage; they still enter the ordinary armor path.
        if (incoming.mFatigue <= 0)
        {
            auto prepared = prepareOrdinaryArmorWear(world, *service, attacker, victim, incoming.mHealth,
                armor.mAbsorbedFraction, settings, block->mConditions,
                block->mAccepted ? std::optional<float>{ESM4::blockedArmorWear(
                    ESM4::armorWear(incoming.mHealth, armor.mAbsorbedFraction, ESM4::buildDurabilitySettings(settings)),
                    block->mFraction, block->mUnarmed)} : std::nullopt);
            if (!prepared)
                return std::nullopt;
            wear = std::move(*prepared);
        }
        return OblivionUnarmedContactDamage{damage.mHealth, damage.mFatigue, block->mDebit, block->mFraction,
            incoming.mHealth, std::move(wear.mConditions), wear.mRandom,
            block->mAccepted, block->mWeaponBlockSound, block->mSounds, wear.mHitArmorClass};
    }

    std::optional<OblivionUnarmedContactDamage> resolveOblivionOrdinaryUnarmedContact(
        MWBase::World& world, const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim,
        float normalizedDifficulty, bool sneaking)
    {
        return resolveOblivionUnarmedMeleeContact(world, attacker, victim, normalizedDifficulty,
            sneaking, ESM4::MeleeStrikeKind::Left);
    }

    namespace
    {

        std::optional<PreparedArmorWear> prepareOrdinaryArmorWear(MWBase::World& world,
            OblivionCombatService& service, const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim, float incoming,
            float absorbedFraction, std::span<const ESM4::GameSetting* const> settings,
            std::span<const OblivionPhysicalConditionChange> prior, std::optional<float> adjustedWear)
        {
            PreparedArmorWear result{{prior.begin(), prior.end()}, {}};
            // Original5FFBE3 bypasses NPC armor selection for Creature bases,
            // including nonzero armor ratings supplied by their AV authority.
            if (victim.getType() == ESM::REC_CREA4)
                return result;
            if (absorbedFraction <= 0)
                return result;
            const float wear = adjustedWear.value_or(
                ESM4::armorWear(incoming, absorbedFraction, ESM4::buildDurabilitySettings(settings)));
            if (wear <= 0)
                return result;
            const bool player = victim == world.getPlayerPtr();
            const auto key = player ? ESM::FormKey::dynamic("player", 1) : victim.getCellRef().getFormKey();
            const auto* values = service.findActorValues(key);
            if (!values)
                throw std::invalid_argument("native armor wear requires actor-value authority");
            struct Candidate { const ESM4::Armor* mBase = nullptr; MWWorld::Ptr mItem; };
            std::array<Candidate, 7> candidates{};
            auto& inventory = victim.getClass().getInventoryStore(victim);
            for (auto item = inventory.begin(); item != inventory.end(); ++item)
            {
                if (!inventory.isEquipped(*item) || item->getType() != ESM::REC_ARMO)
                    continue; // Original486790(slot,1) excludes clothing.
                // Original selection runs after blocking wear has already
                // removed a broken shield. Inspect that projected inventory.
                if (std::any_of(prior.begin(), prior.end(), [&](const auto& change) {
                    return change.mItem == *item && change.mBreakDisposition != OblivionWeaponBreakDisposition::None;
                })) continue;
                const auto id = MWWorld::OblivionProfileServices::nativeItemId(world.getStore(), item->getCellRef().getRefId());
                const auto* form = id.getIf<ESM::FormId>();
                const auto* armor = form ? world.getStore().get<ESM4::Armor>().search(*form) : nullptr;
                if (!armor)
                    throw std::invalid_argument("native armor wear has no winning armor definition");
                for (unsigned slot = 0; slot < candidates.size(); ++slot)
                {
                    const unsigned nativeSlot = slot == 6 ? 13 : slot;
                    if (!(armor->mArmorFlags & (1u << nativeSlot)))
                        continue;
                    if (slot == 6)
                    {
                        const auto carried = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedLeft);
                        if (values->mProcess != ESM4::ActorValueProcess::Active
                            || carried == inventory.end() || *carried != *item)
                            continue;
                    }
                    if (candidates[slot].mBase)
                        throw std::invalid_argument("overlapping native equipped armor wear slots");
                    candidates[slot] = {armor, *item};
                }
            }
            std::array<bool, 7> available{};
            for (unsigned i = 0; i < available.size(); ++i)
                available[i] = candidates[i].mBase != nullptr;
            const auto selected = ESM4::selectArmorWear(service.combatRandomState(), available,
                ESM4::buildArmorWearSelectionSettings(settings));
            result.mRandom = service.prepareCombatRandom(selected.mDraws);
            if (result.mRandom->mNextState != selected.mNextState)
                throw std::logic_error("native armor selection random preparation mismatch");
            if (!selected.mSlot)
                return result; // All seven failed draws still belong to this contact.
            const auto& candidate = candidates[static_cast<unsigned>(*selected.mSlot)];
            result.mHitArmorClass = (candidate.mBase->mGeneralFlags & ESM4::Armor::TES4_HeavyArmor) ? 1 : 0;
            if (candidate.mBase->mArmorFlags & ESM4::Armor::TES4_Shield)
            {
                // Original5FFC3D queries the attacker (EDI), after selection.
                // The selected material and consumed draws still apply when
                // its posture/mastery gate bypasses this second shield wear.
                if (!oblivionBlockingPosture(world, attacker))
                    return result;
                const auto block = attacker == world.getPlayerPtr() ? service.getPlayerBaseValue(15)
                    : service.getNonPlayerBaseValue(attacker.getCellRef().getFormKey(), 15, world.getStore());
                if (ESM4::combatMastery(block, ESM4::buildCombatMasterySettings(settings))
                    >= ESM4::CombatMastery::Journeyman)
                    return result;
            }
            if (player && world.getGodModeState())
                return result; // Original65FF10 suppresses condition after selection has consumed its draws.
            const auto& ref = candidate.mItem.getCellRef();
            const auto priorCondition = std::find_if(result.mConditions.begin(), result.mConditions.end(),
                [&](const auto& change) { return change.mItem == candidate.mItem; });
            const double current = priorCondition != result.mConditions.end() ? double(priorCondition->mCondition)
                : ref.getNativeItemCondition() ? double(*ref.getNativeItemCondition())
                : ref.getCharge() < 0 ? double(candidate.mBase->mData.health)
                : double(ref.getCharge()) + ref.getChargeIntRemainder();
            const bool heavy = candidate.mBase->mGeneralFlags & ESM4::Armor::TES4_HeavyArmor;
            const auto skill = player ? service.getPlayerBaseValue(heavy ? 18 : 27)
                : service.getNonPlayerBaseValue(key, heavy ? 18 : 27, world.getStore());
            const auto condition = ESM4::nativeArmorConditionAfterWear(current, wear, skill,
                heavy ? ESM4::ArmorWeight::Heavy : ESM4::ArmorWeight::Light,
                ESM4::buildArmorWearMasterySettings(settings), ESM4::buildCombatMasterySettings(settings));
            if (!condition)
                throw std::logic_error("admitted native armor wear did not prepare a condition");
            // Original5F39C7 invalidates armor rating through60E260. Only
            // slot13 enters the equipment consequence branch at5F3A10;
            // ordinary body armor remains equipped when its condition is zero.
            auto change = priorCondition != result.mConditions.end() ? *priorCondition
                : captureOblivionPhysicalConditionChange(victim, candidate.mItem, *condition);
            change.mCondition = *condition;
            if (*condition == 0 && (candidate.mBase->mArmorFlags & ESM4::Armor::TES4_Shield))
                change.mBreakDisposition = shieldBreakDisposition(world, service, victim, *candidate.mBase);
            // Preserve the original expected condition when two wear writes
            // merge. Original5FF914 captured shield sound before this later
            // break, so its material survives the equipment consequence.
            if (priorCondition != result.mConditions.end())
                *priorCondition = std::move(change);
            else
                result.mConditions.push_back(std::move(change));
            return result;
        }
    }

    static float weaponContactDamage(MWBase::World& world, const MWWorld::Ptr& attacker,
        const MWWorld::Ptr& item, ESM4::MeleeStrikeKind kind);

    std::optional<OblivionWeaponContactDamage> resolveOblivionWeaponMeleeContact(
        MWBase::World& world, const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim,
        const MWWorld::Ptr& item, float normalizedDifficulty, bool sneaking, ESM4::MeleeStrikeKind kind)
    {
        if (!std::isfinite(normalizedDifficulty) || normalizedDifficulty < -1 || normalizedDifficulty > 1)
            throw std::invalid_argument("native weapon contact difficulty outside normalized slider domain");
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        const auto player = world.getPlayerPtr();
        const auto eligible = [&](const MWWorld::Ptr& ptr) {
            return !ptr.isEmpty() && (ptr == player || ptr.getType() == ESM::REC_NPC_4);
        };
        const bool creatureVictim = !victim.isEmpty() && victim.getType() == ESM::REC_CREA4;
        if (!service || !eligible(attacker) || (!eligible(victim) && !creatureVictim)
            || attacker == victim || sneaking)
            return std::nullopt;
        // Validate authority before class queries can initialize shared caches.
        if (victim == player) (void)service->getPlayerValue(8);
        else (void)service->getNonPlayerValue(victim, 8);
        const auto resistance = victim == player ? service->getPlayerIntegerValue(65)
            : service->getNonPlayerIntegerValue(victim, 65);
        if (resistance != 0)
            return std::nullopt;
        if (kind > ESM4::MeleeStrikeKind::StandingPower) return std::nullopt;
        const float incoming = weaponContactDamage(world, attacker, item, kind);
        if (creatureHasSpells(victim))
            return std::nullopt; // Do not omit Creature passive/contact-effect consequences.
        if (incoming < 0)
            return std::nullopt; // Signed bonus/nonpositive sink policy needs its own branch.
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& record : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(record.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(record.mId));
        const auto id = MWWorld::OblivionProfileServices::nativeItemId(world.getStore(), item.getCellRef().getRefId());
        const auto* weapon = world.getStore().get<ESM4::Weapon>().search(*id.getIf<ESM::FormId>());
        if (!weapon->mEnchantment.isZeroOrUnset())
            return std::nullopt; // Do not consume an enchanted hit before its typed M16 effect hook exists.
        const auto durability = ESM4::buildDurabilitySettings(settings);
        std::optional<float> condition;
        auto breakDisposition = OblivionWeaponBreakDisposition::None;
        if (!(attacker == player && world.getGodModeState()))
        {
            const auto& ref = item.getCellRef();
            const double current = ref.getNativeItemCondition() ? double(*ref.getNativeItemCondition())
                : ref.getCharge() < 0 ? double(weapon->mData.health)
                : double(ref.getCharge()) + ref.getChargeIntRemainder();
            condition = ESM4::nativeConditionAfterWear(current, ESM4::weaponWear(weapon->mData.damage, durability));
            if (condition && *condition == 0)
                breakDisposition = weaponBreakDisposition(world, *service, attacker, *weapon);
        }
        const auto armor = ESM4::mitigateArmor(incoming, oblivionArmorRating(world, victim),
            ESM4::buildArmorRatingSettings(settings).mSkillMaximum, false);
        const auto block = prepareOrdinaryBlock(world, *service, attacker, victim,
            armor.mHealthDamage, static_cast<std::int32_t>(weapon->mData.type), settings);
        if (!block)
            return std::nullopt;
        const auto role = victim == player ? ESM4::PlayerDamageRole::Victim
            : attacker == player ? ESM4::PlayerDamageRole::Attacker : ESM4::PlayerDamageRole::Unaffected;
        const auto damage = ESM4::physicalContactDamage({incoming, 0}, block->mRemaining,
            normalizedDifficulty, ESM4::buildDifficultyDamageMultiplier(settings), role);
        auto wear = prepareOrdinaryArmorWear(world, *service, attacker, victim, incoming, armor.mAbsorbedFraction, settings,
            block->mConditions, block->mAccepted ? std::optional<float>{ESM4::blockedArmorWear(
                ESM4::armorWear(incoming, armor.mAbsorbedFraction, ESM4::buildDurabilitySettings(settings)),
                block->mFraction, block->mUnarmed)} : std::nullopt);
        if (!wear)
            return std::nullopt;
        return OblivionWeaponContactDamage{damage.mHealth, condition, std::move(wear->mConditions), wear->mRandom,
            incoming, block->mDebit, block->mFraction, block->mAccepted, block->mWeaponBlockSound, block->mSounds,
            wear->mHitArmorClass, breakDisposition};
    }

    static float weaponContactDamage(MWBase::World& world,
        const MWWorld::Ptr& attacker, const MWWorld::Ptr& item, ESM4::MeleeStrikeKind kind)
    {
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        const auto player = world.getPlayerPtr();
        if (!service || attacker.isEmpty() || (attacker != player && attacker.getType() != ESM::REC_NPC_4)
            || item.isEmpty() || item.getType() != ESM::REC_WEAP)
            throw std::invalid_argument("native ordinary weapon query requires a Player/NPC and equipped weapon");
        // Reject absent or mismatched authority before the shared class can
        // lazily construct its inventory/custom-data cache.
        if (attacker == player)
            (void)service->getPlayerValue(8);
        else
            (void)service->getNonPlayerValue(attacker, 8);
        auto& inventory = attacker.getClass().getInventoryStore(attacker);
        const auto carried = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        if (item.getContainerStore() != &inventory || carried == inventory.end() || *carried != item)
            throw std::invalid_argument("native ordinary weapon query requires the attacker's equipped instance");
        const auto id = MWWorld::OblivionProfileServices::nativeItemId(world.getStore(), item.getCellRef().getRefId());
        const auto* form = id.getIf<ESM::FormId>();
        const auto* weapon = form ? world.getStore().get<ESM4::Weapon>().search(*form) : nullptr;
        if (!weapon || weapon->mData.type > 3 || weapon->mData.health == 0)
            throw std::invalid_argument("native ordinary weapon query requires a finite-domain melee definition");
        const auto current = [&](std::uint8_t av) { return attacker == player ? service->getPlayerValue(av)
            : service->getNonPlayerValue(attacker, av); };
        const auto integer = [&](std::uint8_t av) {
            const double value = std::trunc(double(current(av)));
            if (!std::isfinite(value) || value < std::numeric_limits<std::int32_t>::min()
                || value > std::numeric_limits<std::int32_t>::max())
                throw std::invalid_argument("native weapon float AV conversion exceeds int32 domain");
            return static_cast<std::int32_t>(value);
        };
        // Original item reader returns double. Missing condition means the
        // unsigned original maximum, before division and the float ratio store.
        const auto& ref = item.getCellRef();
        const double condition = ref.getNativeItemCondition() ? double(*ref.getNativeItemCondition())
            : ref.getCharge() < 0 ? double(weapon->mData.health)
            : double(ref.getCharge()) + ref.getChargeIntRemainder();
        if (!std::isfinite(condition) || condition < 0)
            throw std::invalid_argument("invalid native weapon condition");
        const float ratio = static_cast<float>(condition / weapon->mData.health);
        const auto key = attacker == player ? ESM::FormKey::dynamic("player", 1) : attacker.getCellRef().getFormKey();
        const auto baseFatigue = attacker == player ? service->getPlayerBaseValue(10)
            : service->getNonPlayerBaseValue(key, 10, world.getStore());
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& record : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(record.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(record.mId));
        const float damage = ESM4::weaponDamage({integer(weapon->mData.type < 2 ? 14 : 16), integer(7),
            integer(0), weapon->mData.damage, ratio, ESM4::combatFatigueRatio(current(10), baseFatigue),
            contactPowerMultiplier(world, *service, attacker, weapon->mData.type < 2 ? 14 : 16, kind, settings)},
            ESM4::buildPhysicalCombatSettings(settings));
        const auto bonus = attacker == player ? service->getPlayerIntegerValue(42)
            : service->getNonPlayerIntegerValue(attacker, 42);
        const float total = ESM4::addWeaponAttackBonus(damage, bonus);
        if (!std::isfinite(total))
            throw std::invalid_argument("nonfinite native weapon damage");
        return total;
    }

    std::optional<OblivionWeaponContactDamage> resolveOblivionOrdinaryWeaponContact(
        MWBase::World& world, const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim,
        const MWWorld::Ptr& item, float normalizedDifficulty, bool sneaking)
    {
        return resolveOblivionWeaponMeleeContact(world, attacker, victim, item,
            normalizedDifficulty, sneaking, ESM4::MeleeStrikeKind::Left);
    }

    float oblivionOrdinaryWeaponContactDamage(MWBase::World& world,
        const MWWorld::Ptr& attacker, const MWWorld::Ptr& item)
    {
        return weaponContactDamage(world, attacker, item, ESM4::MeleeStrikeKind::Left);
    }

    float oblivionArmorRating(MWBase::World& world, const MWWorld::Ptr& actor)
    {
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        if (!service || actor.isEmpty())
            throw std::invalid_argument("native armor query requires a native actor");
        const bool player = actor == world.getPlayerPtr();
        if (!player && actor.getType() != ESM::REC_NPC_4 && actor.getType() != ESM::REC_CREA4)
            throw std::invalid_argument("native armor query requires a native actor");
        const auto key = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
        const auto* values = service->findActorValues(key);
        if (!values)
            throw std::invalid_argument("native armor query requires actor-value authority");
        const auto value = [&](std::uint8_t index) { return player ? service->getPlayerValue(index)
            : service->getNonPlayerValue(actor, index); };
        const float defense = value(43);
        if (actor.getType() == ESM::REC_CREA4)
            return defense; // Original Creature virtual5E0CD0 reads AV2B directly.
        const auto integer = [&](std::uint8_t index) {
            // Armor's caller uses float AV queries then9828C0, rather than
            // the per-modifier integer actor-value getter or mastery flooring.
            const double current = std::trunc(double(value(index)));
            if (!std::isfinite(current) || current < std::numeric_limits<std::int32_t>::min()
                || current > std::numeric_limits<std::int32_t>::max())
                throw std::invalid_argument("native armor AV conversion exceeds supported int32 domain");
            return static_cast<std::int32_t>(current);
        };
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seenSettings;
        for (const auto& record : world.getStore().get<ESM4::GameSetting>())
            if (seenSettings.insert(record.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(record.mId));
        const auto physical = ESM4::buildPhysicalCombatSettings(settings);
        const auto rating = ESM4::buildArmorRatingSettings(settings);
        const auto mastery = ESM4::buildCombatMasterySettings(settings);
        const auto armorMastery = ESM4::buildArmorMasterySettings(settings);
        const float maximum = ESM4::buildMaximumArmorRating(settings);
        struct EquippedArmor { const ESM4::Armor* mBase = nullptr; MWWorld::Ptr mItem; };
        std::array<EquippedArmor, 16> slots{};
        auto& inventory = actor.getClass().getInventoryStore(actor);
        for (auto item = inventory.begin(); item != inventory.end(); ++item)
        {
            if (!inventory.isEquipped(*item))
                continue;
            const auto nativeId = MWWorld::OblivionProfileServices::nativeItemId(
                world.getStore(), item->getCellRef().getRefId());
            const auto* form = nativeId.getIf<ESM::FormId>();
            const auto* armor = form ? world.getStore().get<ESM4::Armor>().search(*form) : nullptr;
            if (!armor)
            {
                if (item->getType() == ESM::REC_ARMO)
                    throw std::invalid_argument("projected armor has no winning native definition");
                continue;
            }
            std::uint32_t mask = armor->mArmorFlags & 0xffffu;
            if (mask == (ESM4::Armor::TES4_LeftRing | ESM4::Armor::TES4_RightRing))
            {
                const auto left = inventory.getSlot(MWWorld::InventoryStore::Slot_LeftRing);
                const auto right = inventory.getSlot(MWWorld::InventoryStore::Slot_RightRing);
                if (left != inventory.end() && *left == *item) mask = ESM4::Armor::TES4_LeftRing;
                else if (right != inventory.end() && *right == *item) mask = ESM4::Armor::TES4_RightRing;
                else throw std::invalid_argument("native armor ring lacks its projected selected slot");
            }
            for (unsigned slot = 0; slot < slots.size(); ++slot)
                if (mask & (1u << slot))
                {
                    if (slots[slot].mBase)
                        throw std::invalid_argument("overlapping native equipped armor slots");
                    slots[slot] = {armor, *item};
                }
        }
        // Native slot13 uses the process's actually worn shield entry. Low
        // processes expose no shield entry; drawing/held block is not this test.
        const auto carried = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedLeft);
        if (values->mProcess != ESM4::ActorValueProcess::Active || carried == inventory.end()
            || slots[13].mItem != *carried)
            slots[13] = {};
        std::array<bool, 7> light{}, heavy{};
        for (unsigned i = 0; i < light.size(); ++i)
        {
            const auto* armor = slots[i == 6 ? 13 : i].mBase;
            if (armor)
                ((armor->mGeneralFlags & ESM4::Armor::TES4_HeavyArmor) ? heavy : light)[i] = true;
        }
        float lightRating = 0, heavyRating = 0;
        std::set<ESM::FormId> seenArmor;
        const auto luck = integer(7);
        for (unsigned slot = 0; slot < slots.size(); ++slot)
        {
            const auto& equipped = slots[slot];
            if (!equipped.mBase || (slot != 13 && !seenArmor.insert(equipped.mBase->mId).second))
                continue;
            const auto& armor = *equipped.mBase;
            const bool isHeavy = armor.mGeneralFlags & ESM4::Armor::TES4_HeavyArmor;
            const float full = static_cast<float>(armor.mData.health);
            const float condition = equipped.mItem.getCellRef().getItemCondition(full);
            const float ratio = full == 0 ? 0 : static_cast<float>(double(condition) / full);
            const float amount = ESM4::nativeEquippedArmorRating({armor.mData.armor, integer(isHeavy ? 18 : 27), luck, ratio},
                rating, physical);
            auto& total = isHeavy ? heavyRating : lightRating;
            total = static_cast<float>(double(total) + amount);
        }
        const float itemRating = static_cast<float>(double(lightRating) + heavyRating);
        const auto baseLight = player ? service->getPlayerBaseValue(27)
            : service->getNonPlayerBaseValue(key, 27, world.getStore());
        const float mastered = ESM4::masteryArmorRating(itemRating, 0, baseLight,
            ESM4::armorCoverage(light, armorMastery), ESM4::armorCoverage(heavy, armorMastery),
            0, armorMastery, mastery);
        const float total = static_cast<float>(double(mastered) + defense);
        if (!std::isfinite(total))
            throw std::invalid_argument("nonfinite native armor aggregate");
        // Original total cap has no lower clamp; signed DefendBonus is valid.
        return maximum > 0 ? std::min(total, maximum) : total;
    }

}
