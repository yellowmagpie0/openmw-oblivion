#include "oblivionmelee.hpp"
#include "oblivionplayerhostility.hpp"

#include "oblivioncombat.hpp"
#include "oblivionai.hpp"
#include "creaturestats.hpp"
#include "movement.hpp"
#include "steering.hpp"
#include "../mwworld/oblivioncombatdata.hpp"
#include "../mwworld/oblivionactorstats.hpp"
#include "../mwworld/oblivioncombatmagic.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwsound/nativeaudioutils.hpp"
#include <components/debug/debuglog.hpp>
#include "../mwbase/world.hpp"
#include "../mwphysics/actor.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/oblivionprofileservices.hpp"
#include "../mwworld/oblivionscriptmanager.hpp"
#include <components/esm3/loadarmo.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/loadench.hpp>
#include <components/esm4/loadmgef.hpp>
#include <components/esm4/loadspel.hpp>
#include <components/esm4/loadlvsp.hpp>
#include <components/esm4/runtimereferences.hpp>
#include <components/esm4/weaponenchantment.hpp>
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
#include <variant>
#include <vector>

namespace MWMechanics
{
    namespace
    {
        // A bounded physical damage-rate policy shared with the existing
        // disarm fallback. Combat melee excludes staves, bows and ammunition.
        MWWorld::Ptr selectUsablePhysicalWeapon(MWBase::World& world, const MWWorld::Ptr& actor,
            const MWWorld::Ptr& removed, std::uint32_t maximumType)
        {
            auto& inventory = actor.getClass().getInventoryStore(actor);
            MWWorld::Ptr selected;
            double best = -1;
            for (auto item = inventory.begin(); item != inventory.end(); ++item)
            {
                if (*item == removed || item->getType() != ESM::REC_WEAP
                    || item->getCellRef().getCount() <= 0) continue;
                const auto id = MWWorld::OblivionProfileServices::nativeItemId(
                    world.getStore(), item->getCellRef().getRefId());
                const auto* weapon = world.getStore().get<ESM4::Weapon>().search(id);
                if (!weapon || weapon->mData.type > maximumType) continue;
                const auto& ref = item->getCellRef();
                const double condition = ref.getNativeItemCondition().value_or(
                    ref.getCharge() < 0 ? float(weapon->mData.health)
                                        : float(ref.getCharge()) + ref.getChargeIntRemainder());
                if (!std::isfinite(condition) || condition < 0 || !std::isfinite(weapon->mData.speed)
                    || weapon->mData.speed <= 0 || (weapon->mData.health && condition == 0)) continue;
                const double ratio = weapon->mData.health ? condition / weapon->mData.health : 1;
                const double rating = weapon->mData.damage * ratio * weapon->mData.speed;
                if (rating > best)
                {
                    best = rating;
                    selected = *item;
                }
            }
            return selected;
        }

        void equipOwnedMeleeWeapon(MWWorld::World& world, const MWWorld::Ptr& actor,
            OblivionCombatService& combat)
        {
            if (actor.getType() != ESM::REC_NPC_4) return;
            auto& inventory = actor.getClass().getInventoryStore(actor);
            if (inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight) != inventory.end()) return;
            const auto selected = selectUsablePhysicalWeapon(world, actor, {}, 3);
            if (selected.isEmpty()) return;
            const auto key = actor.getCellRef().getFormKey();
            auto* mechanics = MWBase::Environment::get().getMechanicsManagerOrNull();
            if (!mechanics || !mechanics->cancelOblivionCombatInput(actor))
            {
                combat.endBlocking(key);
                combat.clearMeleeInput(key);
            }
            // Cancellation observers may change the inventory. Re-find the
            // exact physical stack; never substitute another same-base unit.
            const auto found = std::find(inventory.begin(), inventory.end(), selected);
            if (found == inventory.end()
                || inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight) != inventory.end()
                || selectUsablePhysicalWeapon(world, actor, {}, 3) != selected)
                return;
            inventory.equip(MWWorld::InventoryStore::Slot_CarriedRight, found);
            Log(Debug::Info) << "M15 combat melee weapon equipped: actor=" << key.serialize()
                             << " item=" << selected.getCellRef().getRefId();
        }
    }

    MWWorld::Ptr selectOblivionDisarmReplacement(
        MWBase::World& world, const MWWorld::Ptr& actor, const MWWorld::Ptr& removed)
    {
        auto* nativeWorld = dynamic_cast<MWWorld::World*>(&world);
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        if (!service || actor.isEmpty() || actor == world.getPlayerPtr()
            || actor.getType() != ESM::REC_NPC_4) return {};
        const auto key = actor.getCellRef().getFormKey();
        const auto* life = service->findActorLife(key);
        // The NPC drop wrapper performs this immediate equip outside its
        // combat package. An active combat controller owns its later choice.
        if (!life || life->mPhase != ESM4::ActorLifePhase::Alive
            || nativeWorld->hasOblivionCombatController(key).value_or(false)) return {};
        return selectUsablePhysicalWeapon(world, actor, removed, 5);
    }

    bool isOblivionStationaryMeleeStyle(const ESM4::CombatStyleStandard& style)
    {
        constexpr auto stationaryFlags = static_cast<std::uint8_t>(ESM4::CombatStyleFlag::ChooseAttackChance)
            | static_cast<std::uint8_t>(ESM4::CombatStyleFlag::DisableFleeing);
        return style.mFlags == stationaryFlags && style.mAttackChance == 100
            && style.mBlockChance == 0 && style.mDodgeChance == 0 && style.mAcrobaticDodgeChance == 0
            && (style.mPowerAttackChance == 0 || style.mPowerAttackChance == 100)
            && style.mIdle.mMinimum == 0 && style.mIdle.mMaximum == 0
            && style.mHold.mMinimum == 0 && style.mHold.mMaximum == 0
            && style.mAttackRecoilBonus == 0 && style.mAttackUnconsciousBonus == 0 && style.mAttackUnarmedBonus == 0
            && style.mPowerAttackRecoilBonus == 0 && style.mPowerAttackUnconsciousBonus == 0
            && style.mDoNotAcquire;
    }

    ESM4::MeleeStrikeKind selectOblivionStationaryMeleeStrike(
        const ESM4::CombatStyleStandard& style, bool preferLeft)
    {
        if (!isOblivionStationaryMeleeStyle(style))
            throw std::invalid_argument("unsupported stationary melee selection style");
        // The S4 policy admits deterministic endpoints only. A power intent
        // starts its actual power sequence, without a preliminary ordinary hit
        // or a fabricated held-input delay.
        return style.mPowerAttackChance == 100 ? ESM4::MeleeStrikeKind::StandingPower
            : preferLeft ? ESM4::MeleeStrikeKind::Left : ESM4::MeleeStrikeKind::Right;
    }

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

    static std::optional<std::int32_t> oblivionFightScore(
        MWWorld::World& world, const MWWorld::Ptr& actor, const MWWorld::Ptr& target)
    {
        auto* combat = world.getOblivionCombatService();
        if (world.getGameProfile() != ESM::GameProfile::Oblivion || !combat
            || actor.isEmpty() || target.isEmpty() || actor == target
            || (actor.getType() != ESM::REC_NPC_4 && actor.getType() != ESM::REC_CREA4))
            return std::nullopt;
        const auto key = actor.getCellRef().getFormKey();
        if (!combat->findActorValues(key)) return std::nullopt;
        const auto aggression = combat->getNonPlayerIntegerValue(actor, 33);
        if (aggression <= 0)
            return 0;
        const auto targetKey = target == world.getPlayerPtr() ? ESM::FormKey::dynamic("player", 1)
            : target.getCellRef().getFormKey();
        const auto disposition = world.getOblivionDisposition(key, targetKey);
        if (!disposition)
            return std::nullopt;
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& setting : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(setting.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(setting.mId));
        const auto distance = (actor.getRefData().getPosition().asVec3()
            - target.getRefData().getPosition().asVec3()).length();
        // Original 524AD4/5EA187 only admit positive 546190 fight scores.
        // Ordinary hostility has no ally incident/friend term.
        // Detection and target selection belong to the acquisition caller.
        const ESM4::FightScoreInput input{*disposition, 0, aggression, distance,
            false, false, combat->getNonPlayerIntegerValue(actor, 36)};
        return ESM4::fightScore(input, ESM4::buildFightScoreSettings(settings));
    }

    bool acquireOblivionCombat(MWBase::World& world, const MWWorld::Ptr& actor,
        std::span<const MWWorld::Ptr> candidates)
    {
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* combat = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        auto* ai = nativeWorld ? nativeWorld->getOblivionAiService() : nullptr;
        if (!combat || !ai || actor.isEmpty()
            || (actor.getType() != ESM::REC_NPC_4 && actor.getType() != ESM::REC_CREA4))
            return false;
        if (!actor.isInCell() || !actor.getRefData().isEnabled() || actor.getBase()->isDeleted()
            || actor == world.getPlayerPtr())
            return false;
        const auto key = actor.getCellRef().getFormKey();
        if (nativeWorld->hasOblivionCombatController(key).value_or(false))
            return false;
        const auto* values = combat->findActorValues(key);
        const auto* life = combat->findActorLife(key);
        if (!values || values->mProcess != ESM4::ActorValueProcess::Active
            || !life || life->mPhase != ESM4::ActorLifePhase::Alive
            || ai->isRestrained(actor) || ai->isRidingHorse(actor)
            || oblivionParalyzed(world, actor) || oblivionKnockedState(world, actor) != 0)
            return false;
        if (const auto* state = ai->state(actor); state && state->mPackageType == ESM4::AIPackageType::Sleep
            && (state->mPhase == ESM4::PackagePhase::Act || state->mPhase == ESM4::PackagePhase::Wait))
            return false;
        const auto player = world.getPlayerPtr();
        const auto candidateKey = [&](const MWWorld::Ptr& candidate) {
            return candidate == player ? ESM::FormKey::dynamic("player", 1)
                : candidate.getCellRef().getFormKey();
        };
        MWWorld::Ptr selected;
        float nearest = std::numeric_limits<float>::infinity();
        for (const auto& target : candidates)
        {
            if (target.isEmpty() || target == actor || !target.isInCell()
                || !target.getRefData().isEnabled() || target.getBase()->isDeleted()
                || (target != player && target.getType() != ESM::REC_NPC_4 && target.getType() != ESM::REC_CREA4)
                || (actor.getCell() != target.getCell()
                    && (!actor.getCell()->isExterior() || !target.getCell()->isExterior())))
                continue;
            const auto targetKey = candidateKey(target);
            const auto* targetValues = combat->findActorValues(targetKey);
            const auto* targetLife = combat->findActorLife(targetKey);
            if (!targetValues || targetValues->mProcess != ESM4::ActorValueProcess::Active
                || !targetLife || targetLife->mPhase != ESM4::ActorLifePhase::Alive)
                continue;
            const float distance = (actor.getRefData().getPosition().asVec3()
                - target.getRefData().getPosition().asVec3()).length2();
            if (distance > nearest || (distance == nearest && !selected.isEmpty()
                    && !(targetKey < candidateKey(selected))))
                continue;
            const auto score = oblivionFightScore(*nativeWorld, actor, target);
            if (!score || *score <= 0 || !ai->detection(actor, target).mDetected)
                continue;
            selected = target;
            nearest = distance;
        }
        // Practical nearest detected hostile policy, not original target ranking.
        // Publish only the selected controller; rejected candidates consume no RNG.
        return !selected.isEmpty() && nativeWorld->executeOblivionStartCombat(actor, selected);
    }

    std::optional<std::int32_t> oblivionPlayerFightScore(
        MWWorld::World& world, const MWWorld::Ptr& actor)
    {
        return oblivionFightScore(world, actor, world.getPlayerPtr());
    }

    bool acquireOblivionPlayerCombat(MWBase::World& world, const MWWorld::Ptr& actor)
    {
        const std::array candidates{world.getPlayerPtr()};
        return acquireOblivionCombat(world, actor, candidates);
    }

    bool updateOblivionMeleeAi(MWBase::World& world, const MWWorld::Ptr& actor, bool enabled, float duration)
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
        const auto currentTarget = nativeWorld->getOblivionCombatTarget(key);
        const bool engaged = currentTarget.has_value();
        auto& movement = actor.getClass().getMovementSettings(actor);
        if (engaged)
        {
            std::fill(std::begin(movement.mPosition), std::end(movement.mPosition), 0.f);
            std::fill(std::begin(movement.mRotation), std::end(movement.mRotation), 0.f);
        }
        const auto* values = combat->findActorValues(key);
        const auto* life = combat->findActorLife(key);
        if (!enabled || !engaged || !values || !life || life->mPhase != ESM4::ActorLifePhase::Alive
            || values->mProcess != ESM4::ActorValueProcess::Active || ai->isRestrained(actor)
            || ai->isRidingHorse(actor) || oblivionParalyzed(world, actor) || oblivionKnockedState(world, actor) != 0)
        {
            combat->clearMeleeAiIntent(key);
            ai->clearCombatRoute(actor);
            return engaged;
        }
        // The current base combat package supersedes the authored coordinator's
        // queued action. Its frozen package intent is not the process override.
        if (const auto* melee = combat->findMeleeState(key); melee && melee->mStrike
            && melee->mStrike->mKind == ESM4::MeleeStrikeKind::Counter)
            return true; // The defensive controller owns its untargeted Attack-key acquisition.
        if (currentTarget->isNull())
        {
            combat->clearMeleeAiIntent(key);
            ai->clearCombatRoute(actor);
            return true; // An allocated-empty controller has no attack target.
        }
        const auto policy = MWWorld::resolveOblivionCombatPolicy(world.getStore(), values->mBase,
            MWWorld::buildOblivionCombatDefaults(world.getStore()));
        const auto& style = policy.mStandard;
        const bool stationary = isOblivionStationaryMeleeStyle(style);
        const auto target = *currentTarget == ESM::FormKey::dynamic("player", 1)
            ? world.getPlayerPtr() : ai->resolveReference(*currentTarget);
        const auto* targetLife = combat->findActorLife(*currentTarget);
        if (target.isEmpty() || !target.isInCell() || !target.getRefData().isEnabled()
            || !targetLife || targetLife->mPhase != ESM4::ActorLifePhase::Alive)
        {
            combat->clearMeleeAiIntent(key);
            ai->clearCombatRoute(actor);
            return true;
        }
        const auto* physics = dynamic_cast<const MWPhysics::PhysicsSystem*>(world.getRayCasting());
        if (!physics || !physics->getActor(actor) || !physics->getActor(target))
        {
            combat->clearMeleeAiIntent(key);
            ai->clearCombatRoute(actor);
            return true; // Lost collision residence cannot turn into a substituted hit or cost.
        }
        const auto* priorMelee = combat->findMeleeState(key);
        auto& staffInventory = actor.getClass().getInventoryStore(actor);
        auto staffItem = staffInventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        const auto staffInputs = [&](const MWWorld::Ptr& item) -> std::optional<MWWorld::OblivionStaffCastInputs> {
            if (item.getType() != ESM::REC_WEAP) return {};
            const auto id = MWWorld::OblivionProfileServices::nativeItemId(world.getStore(), item.getCellRef().getRefId());
            const auto* weapon = world.getStore().get<ESM4::Weapon>().search(id);
            if (!weapon || item.getCellRef().getItemCondition(weapon->mData.health) <= 0) return {};
            return MWWorld::resolveOblivionStaffCastInputs(world.getStore(), *weapon);
        };
        if (staffItem == staffInventory.end() && !combat->findSpellCast(key)
            && (!priorMelee || (!priorMelee->mStrike && !priorMelee->mReaction)))
            for (auto item = staffInventory.begin(); item != staffInventory.end(); ++item)
                if (const auto inputs = staffInputs(*item))
                {
                    const float charge = item->getCellRef().getEnchantmentCharge();
                    if ((charge < 0 ? inputs->mMaximumCharge : charge) < inputs->mAuthoredChargeCost) continue;
                    staffInventory.equip(MWWorld::InventoryStore::Slot_CarriedRight, item);
                    staffItem = staffInventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
                    Log(Debug::Info) << "M16 staff equipped: actor=" << key.serialize();
                    break;
                }
        if (staffItem != staffInventory.end())
            if (const auto inputs = staffInputs(*staffItem))
            {
                const float charge = staffItem->getCellRef().getEnchantmentCharge();
                if ((charge < 0 ? inputs->mMaximumCharge : charge) >= inputs->mAuthoredChargeCost
                    || combat->findSpellCast(key))
                {
                    if (nativeWorld->getOblivionCombatTarget(key) != currentTarget) return true;
                    combat->engage(key, *currentTarget);
                    combat->clearMeleeAiIntent(key);
                    stats.setDrawState(DrawState::Weapon);
                    const auto delta = target.getRefData().getPosition().asVec3()
                        - actor.getRefData().getPosition().asVec3();
                    static_cast<void>(zTurn(actor, std::atan2(delta.x(), delta.y())));
                    const bool visible = world.getLOS(actor, target);
                    if (visible) ai->clearCombatRoute(actor);
                    else ai->approachCombatTarget(actor, target, duration);
                    stats.setAttackingOrSpell(visible && style.mAttackChance != 0);
                    return true;
                }
                // An exhausted staff cannot be treated as an ordinary melee weapon.
                staffInventory.unequipSlot(MWWorld::InventoryStore::Slot_CarriedRight);
            }
        if (const auto* afterEquipment = combat->findMeleeState(key); !afterEquipment || !afterEquipment->mStrike)
            equipOwnedMeleeWeapon(*nativeWorld, actor, *combat);
        // Equipment observers can stop combat or replace its target.
        if (nativeWorld->getOblivionCombatTarget(key) != currentTarget)
        {
            combat->clearMeleeAiIntent(key);
            ai->clearCombatRoute(actor);
            return nativeWorld->hasOblivionCombatController(key).value_or(false);
        }
        // StopCombat on the other actor clears symmetric bookkeeping, not
        // this actor's directed controller. Reconcile the live owned edge
        // before publishing an intent whose persistence requires that edge.
        combat->engage(key, *currentTarget);
        combat->setMeleeAiIntent(key, {*currentTarget, policy.mStyle}, world.getStore());
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
        if (!stationary)
        {
            const auto delta = target.getRefData().getPosition().asVec3()
                - actor.getRefData().getPosition().asVec3();
            static_cast<void>(zTurn(actor, std::atan2(delta.x(), delta.y())));
        }
        const bool reachable = canReachOblivionMeleeTarget(world, actor, target, reach);
        if (!stationary && !reachable)
            ai->approachCombatTarget(actor, target, duration);
        else
            ai->clearCombatRoute(actor);
        stats.setAttackingOrSpell(reachable && style.mAttackChance != 0);
        return true;
    }

    std::optional<OblivionPhysicalContactDeltas> prepareOblivionCounterContactDeltas(
        MWBase::World& world, std::uint64_t actionId, const MWWorld::Ptr& attacker,
        const MWWorld::Ptr& victim)
    {
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        const auto player = world.getPlayerPtr();
        if (!service || attacker.isEmpty()
            || (attacker != player && attacker.getType() != ESM::REC_NPC_4
                && attacker.getType() != ESM::REC_CREA4)) return std::nullopt;
        const auto source = attacker == player ? ESM::FormKey::dynamic("player", 1)
            : attacker.getCellRef().getFormKey();
        const auto* state = service->findMeleeState(source);
        if (!state || !state->mStrike || state->mStrike->mActionId != actionId
            || state->mStrike->mKind != ESM4::MeleeStrikeKind::Counter
            || !service->isMeleeContactPending(actionId, source)
            || service->getProcessAction(source) != 2) return std::nullopt;
        OblivionPhysicalContactDeltas deltas;
        if (victim.isEmpty()) return deltas; // A miss consumes phase1 without any draw or cost.
        if (victim == attacker || (victim != player && victim.getType() != ESM::REC_NPC_4
                && victim.getType() != ESM::REC_CREA4))
            return std::nullopt;
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& setting : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(setting.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(setting.mId));
        auto& inventory = attacker.getClass().getInventoryStore(attacker);
        const auto right = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        const auto left = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedLeft);
        const bool hasWeapon = right != inventory.end() && right->getType() == ESM::REC_WEAP;
        const bool hasShield = left != inventory.end() && left->getType() == ESM::REC_ARMO
            && (left->get<ESM::Armor>()->mBase->mData.mType == ESM::Armor::Shield);
        const auto base = [&](std::uint8_t av) { return attacker == player ? service->getPlayerBaseValue(av)
            : service->getNonPlayerBaseValue(source, av, world.getStore()); };
        const ESM4::BlockMasteryInput mastery{base(15), base(17), hasWeapon, hasShield};
        auto& targetInventory = victim.getClass().getInventoryStore(victim);
        const auto targetRight = targetInventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        const bool targetHasWeapon = targetRight != targetInventory.end() && targetRight->getType() == ESM::REC_WEAP;
        const ESM4::Weapon* targetWeapon = nullptr;
        if (targetHasWeapon)
        {
            const auto id = MWWorld::OblivionProfileServices::nativeItemId(
                world.getStore(), targetRight->getCellRef().getRefId());
            const auto* form = id.getIf<ESM::FormId>();
            targetWeapon = form ? world.getStore().get<ESM4::Weapon>().search(*form) : nullptr;
            if (!targetWeapon) return std::nullopt;
        }
        const ESM4::DisarmTargetWeaponInput target{targetHasWeapon,
            victim.getClass().getCreatureStats(victim).getDrawState() == DrawState::Weapon,
            targetWeapon && (targetWeapon->mFlags & ESM4::Rec_Persistent),
            victim == player && world.getGodModeState()};
        const auto draw = ESM4::combatRandomDraw(service->combatRandomState());
        const auto disarm = ESM4::blockMasteryDisarm(mastery, attacker.getType() != ESM::REC_CREA4, target,
            attacker.getClass().getCreatureStats(attacker).getDrawState() == DrawState::Weapon,
            draw.mValue % 100, ESM4::buildMasteryProcSettings(settings), ESM4::buildCombatMasterySettings(settings));
        if (disarm.mConsumesDraw) deltas.mRandomTransition = service->prepareCombatRandom(1);
        if (disarm.mTriggered)
        {
            deltas.mDisarms.push_back({victim, *targetRight});
        }
        if (!nativeWorld->prepareOblivionStaggerReaction(actionId, victim, deltas)) return std::nullopt;
        return deltas;
    }

    std::optional<OblivionDefensiveBlockConsequences> prepareOblivionDefensiveBlockConsequences(MWBase::World& world,
        const MWWorld::Ptr& attacker, const MWWorld::Ptr& defender,
        OblivionPhysicalContactDeltas& deltas, bool cachedDispatch)
    {
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        const auto player = world.getPlayerPtr();
        if (!service || attacker.isEmpty() || defender.isEmpty() || attacker == defender
            || (defender != player && defender.getType() != ESM::REC_NPC_4
                && defender.getType() != ESM::REC_CREA4)
            || (!cachedDispatch && !oblivionBlockingPosture(world, defender)) || deltas.mVictimCounterRequested || deltas.mVictimCounter)
            return std::nullopt;
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& setting : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(setting.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(setting.mId));
        auto& inventory = defender.getClass().getInventoryStore(defender);
        const auto retained = [&](const auto& slot) {
            return slot != inventory.end()
                && std::none_of(deltas.mConditionChanges.begin(), deltas.mConditionChanges.end(), [&](const auto& change) {
                    return change.mOwner == defender && change.mItem == *slot
                        && change.mBreakDisposition != OblivionWeaponBreakDisposition::None;
                });
        };
        const auto right = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        const auto left = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedLeft);
        const bool hasWeapon = retained(right) && right->getType() == ESM::REC_WEAP;
        const bool hasShield = retained(left) && left->getType() == ESM::REC_ARMO
            && left->get<ESM::Armor>()->mBase->mData.mType == ESM::Armor::Shield;
        const auto key = defender == player ? ESM::FormKey::dynamic("player", 1) : defender.getCellRef().getFormKey();
        const auto base = [&](std::uint8_t av) { return defender == player ? service->getPlayerBaseValue(av)
            : service->getNonPlayerBaseValue(key, av, world.getStore()); };
        const ESM4::BlockMasteryInput input{base(15), base(17), hasWeapon, hasShield};
        const auto mastery = ESM4::buildCombatMasterySettings(settings);
        const bool master = hasWeapon || hasShield
            ? hasShield && ESM4::combatMastery(input.mBlock, mastery) >= ESM4::CombatMastery::Master
            : ESM4::combatMastery(input.mHandToHand, mastery) >= ESM4::CombatMastery::Master;
        const auto draw = ESM4::combatRandomDraw(deltas.mRandomTransition
            ? deltas.mRandomTransition->mNextState : service->combatRandomState());
        const auto proc = ESM4::blockMasteryStagger(input, draw.mValue % 100,
            ESM4::buildMasteryProcSettings(settings), mastery);
        if (!proc.mConsumesDraw) return OblivionDefensiveBlockConsequences{};
        if (attacker != player && attacker.getType() != ESM::REC_NPC_4
            && attacker.getType() != ESM::REC_CREA4)
            return std::nullopt;
        const bool stagger = (deltas.mVictimReaction
            && deltas.mVictimReaction->mKind == ESM4::MeleeReactionKind::Stagger)
            || service->getProcessAction(key) == 8;
        if ((deltas.mVictimReaction && !stagger) || deltas.mAttackerReaction || !deltas.mDisarms.empty())
            return std::nullopt;
        const auto random = service->prepareCombatRandom((deltas.mRandomTransition ? deltas.mRandomTransition->mDraws : 0) + 1);
        if (random.mNextState != draw.mNextState) return std::nullopt;
        auto candidate = deltas;
        candidate.mRandomTransition = random;
        // Original5F4E10 returns before playback when Stagger owns action8,
        // for both BlockHit and Counter. Selection still controls the tail.
        candidate.mVictimCounterRequested = proc.mTriggered && !stagger;
        if (master && !proc.mTriggered)
        {
            // Original600528 first queries Counter even at Master. Only its
            // false branch reaches5FC2B0, whose target query precedes the
            // separate disarm roll and whose source-drawn check follows it.
            auto& targetInventory = attacker.getClass().getInventoryStore(attacker);
            const auto targetRight = targetInventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
            const bool targetHasWeapon = targetRight != targetInventory.end()
                && targetRight->getType() == ESM::REC_WEAP
                && std::none_of(candidate.mConditionChanges.begin(), candidate.mConditionChanges.end(),
                    [&](const auto& change) {
                        return change.mOwner == attacker && change.mItem == *targetRight
                            && change.mBreakDisposition != OblivionWeaponBreakDisposition::None;
                    });
            const ESM4::Weapon* targetWeapon = nullptr;
            if (targetHasWeapon)
            {
                const auto id = MWWorld::OblivionProfileServices::nativeItemId(
                    world.getStore(), targetRight->getCellRef().getRefId());
                const auto* form = id.getIf<ESM::FormId>();
                targetWeapon = form ? world.getStore().get<ESM4::Weapon>().search(*form) : nullptr;
                if (!targetWeapon) return std::nullopt;
            }
            const ESM4::DisarmTargetWeaponInput target{targetHasWeapon,
                attacker.getClass().getCreatureStats(attacker).getDrawState() == DrawState::Weapon,
                targetWeapon && (targetWeapon->mFlags & ESM4::Rec_Persistent),
                attacker == player && world.getGodModeState()};
            const auto next = ESM4::combatRandomDraw(random.mNextState);
            const auto disarm = ESM4::blockMasteryDisarm(input, defender.getType() != ESM::REC_CREA4, target,
                defender.getClass().getCreatureStats(defender).getDrawState() == DrawState::Weapon,
                next.mValue % 100, ESM4::buildMasteryProcSettings(settings), mastery);
            if (disarm.mConsumesDraw)
            {
                const auto combined = service->prepareCombatRandom(random.mDraws + 1);
                if (combined.mNextState != next.mNextState) return std::nullopt;
                candidate.mRandomTransition = combined;
            }
            if (disarm.mTriggered)
            {
                candidate.mDisarms.push_back({attacker, *targetRight});
            }
        }
        deltas = std::move(candidate);
        return OblivionDefensiveBlockConsequences{proc.mTriggered};
    }

    std::optional<bool> prepareOblivionSidePowerConsequences(MWBase::World& world,
        const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim,
        ESM4::MeleeStrikeKind kind, OblivionPhysicalContactDeltas& deltas)
    {
        if (kind != ESM4::MeleeStrikeKind::LeftPower && kind != ESM4::MeleeStrikeKind::RightPower)
            return false;
        const auto player = world.getPlayerPtr();
        auto* nativeWorld = dynamic_cast<MWWorld::World*>(&world);
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        if (!service || attacker.isEmpty() || victim.isEmpty() || attacker == victim
            || !deltas.mOrdinaryHitArguments) return std::nullopt;
        const auto& raw = *deltas.mOrdinaryHitArguments;
        if (!raw.mDamagePath || (attacker != player && attacker.getType() != ESM::REC_NPC_4)) return false;
        if (raw.mUsedSkill != 14 && raw.mUsedSkill != 16 && raw.mUsedSkill != 17) return std::nullopt;
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& setting : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(setting.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(setting.mId));
        const auto mastery = ESM4::buildCombatMasterySettings(settings);
        const auto victimKey = victim == player ? ESM::FormKey::dynamic("player", 1) : victim.getCellRef().getFormKey();
        const auto hand = victim == player ? service->getPlayerBaseValue(17)
            : service->getNonPlayerBaseValue(victimKey, 17, world.getStore());
        if (raw.mBlockAbsorbedFraction > 0
            || (raw.mBareHandBlock && ESM4::combatMastery(hand, mastery) >= ESM4::CombatMastery::Journeyman)) return false;
        auto& inventory = victim.getClass().getInventoryStore(victim);
        const auto right = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        const bool armed = right != inventory.end() && right->getType() == ESM::REC_WEAP
            && std::none_of(deltas.mConditionChanges.begin(), deltas.mConditionChanges.end(), [&](const auto& change) {
                return change.mOwner == victim && change.mItem == *right
                    && change.mBreakDisposition != OblivionWeaponBreakDisposition::None;
            }) && std::none_of(deltas.mDisarms.begin(), deltas.mDisarms.end(), [&](const auto& drop) {
                return drop.mOwner == victim && drop.mItem == *right;
            });
        const ESM4::Weapon* weapon = nullptr;
        if (armed)
        {
            const auto id = MWWorld::OblivionProfileServices::nativeItemId(world.getStore(), right->getCellRef().getRefId());
            const auto* form = id.getIf<ESM::FormId>();
            weapon = form ? world.getStore().get<ESM4::Weapon>().search(*form) : nullptr;
            if (!weapon) return std::nullopt;
        }
        const auto source = attacker == player ? ESM::FormKey::dynamic("player", 1) : attacker.getCellRef().getFormKey();
        const auto baseSkill = attacker == player ? service->getPlayerBaseValue(raw.mUsedSkill)
            : service->getNonPlayerBaseValue(source, raw.mUsedSkill, world.getStore());
        const auto draw = ESM4::combatRandomDraw(deltas.mRandomTransition
            ? deltas.mRandomTransition->mNextState : service->combatRandomState());
        const auto proc = ESM4::sidePowerDisarm(baseSkill,
            kind == ESM4::MeleeStrikeKind::LeftPower ? ESM4::MasteryAttack::Left : ESM4::MasteryAttack::Right, true,
            {armed, victim.getClass().getCreatureStats(victim).getDrawState() == DrawState::Weapon,
                weapon && (weapon->mFlags & ESM4::Rec_Persistent), victim == player && world.getGodModeState()},
            attacker.getClass().getCreatureStats(attacker).getDrawState() == DrawState::Weapon,
            draw.mValue % 100, ESM4::buildMasteryProcSettings(settings), mastery);
        auto candidate = deltas;
        if (proc.mConsumesDraw)
            candidate.mRandomTransition = service->prepareCombatRandom(
                (deltas.mRandomTransition ? deltas.mRandomTransition->mDraws : 0) + 1);
        if (proc.mTriggered)
        {
            candidate.mDisarms.push_back({victim, *right});
            candidate.mKnockback.reset();
        }
        deltas = std::move(candidate);
        return proc.mTriggered;
    }

    void emitOblivionMeleeHitSounds(MWBase::World& world, std::uint64_t action,
        const MWWorld::Ptr& attacker, const OblivionOrdinaryContactResult& contact)
    {
        if (!contact.mHitSounds) return;
        const auto actor = attacker == world.getPlayerPtr() ? ESM::FormKey::dynamic("player", 1)
            : attacker.getCellRef().getFormKey();
        for (const auto editorId : *contact.mHitSounds)
        {
            if (editorId.empty()) continue;
            if (const auto* sound = MWSound::resolveNativeAnimationSound(world.getStore(), editorId))
            {
                const auto manager = MWBase::Environment::get().getSoundManager();
                const bool played = contact.mHitSoundPosition
                    ? manager->playSound3D(osg::Vec3f((*contact.mHitSoundPosition)[0], (*contact.mHitSoundPosition)[1],
                          (*contact.mHitSoundPosition)[2]), sound->mId, 1.f, 1.f) != nullptr
                    : manager->playSound3D(contact.mVictim, sound->mId, 1.f, 1.f) != nullptr;
                Log(Debug::Verbose) << "M15 melee hit sound requested: actor=" << actor.serialize()
                    << " id=" << action << " sound=" << editorId << " played=" << played;
            }
        }
    }

    namespace
    {
        void dispatchCommittedMeleeHit(MWWorld::World& world, const MWWorld::Ptr& attacker,
            const MWWorld::Ptr& victim)
        {
            if (victim.isEmpty()) return;
            if (auto* scripts = world.getOblivionScriptManager())
                scripts->dispatchObjectEvent(victim, "onhit", attacker);

            // A committed attack on the controller's intended opponent is a
            // hostile act. Let a surviving native defender acquire that actor
            // through the same production combat controller. Resolve this after
            // OnHit: an authored Kill or StopCombat may change the live context.
            // Player intervention in an existing fight is hostile as well;
            // Player input never needs an AI controller. A scripted StopCombat
            // still takes precedence. Graph membership alone is insufficient.
            if (victim == world.getPlayerPtr() || attacker.isEmpty()
                || !victim.getRefData().isEnabled() || !attacker.getRefData().isEnabled()
                || victim.getCellRef().getCount() == 0 || attacker.getCellRef().getCount() == 0
                || (victim.getType() != ESM::REC_NPC_4 && victim.getType() != ESM::REC_CREA4))
                return;
            auto* combat = world.getOblivionCombatService();
            if (!combat) return;
            const auto source = attacker == world.getPlayerPtr() ? ESM::FormKey::dynamic("player", 1)
                : ESM4::runtimeReferenceKey(attacker.getCellRef().getFormKey());
            const auto target = ESM4::runtimeReferenceKey(victim.getCellRef().getFormKey());
            const auto* sourceLife = combat->findActorLife(source);
            const auto* targetLife = combat->findActorLife(target);
            if (!sourceLife || !targetLife || sourceLife->mPhase != ESM4::ActorLifePhase::Alive
                || targetLife->mPhase != ESM4::ActorLifePhase::Alive
                || !(world.isOblivionCombatTarget(source, target).value_or(false)
                    || (attacker == world.getPlayerPtr()
                        && world.hasOblivionCombatController(target).value_or(false)))
                || world.isOblivionCombatTarget(target, source).value_or(false)
                || combat->getNonPlayerIntegerValue(victim, 34) <= 0) // Confidence: do not force a coward to fight.
                return;
            if (world.executeOblivionStartCombat(victim, attacker)
                && world.isOblivionCombatTarget(target, source).value_or(false))
                Log(Debug::Info) << "M15 hostile-hit combat response: actor=" << target.serialize()
                                 << " target=" << source.serialize();
        }
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
        if (kind == ESM4::MeleeStrikeKind::Counter)
        {
            std::vector<const ESM4::GameSetting*> settings;
            std::set<ESM::FormId> seen;
            for (const auto& setting : world.getStore().get<ESM4::GameSetting>())
                if (seen.insert(setting.mId).second)
                    settings.push_back(world.getStore().get<ESM4::GameSetting>().search(setting.mId));
            const float placed = attacker.getCellRef().getScale();
            osg::Vec3f scale(placed, placed, placed);
            attacker.getClass().adjustScale(attacker, scale, true);
            // Original5FCC83..5FCCB0 uses reference/race scale times
            // fCombatDistance, without weapon reach or a selected AI target.
            const float counterReach = ESM4::weaponMeleeReach(1.f, scale.z(), ESM4::buildMeleeReachSettings(settings));
            const auto contact = acquireOblivionMeleeContact(world, actionId, attacker, {}, counterReach);
            if (!contact) return std::nullopt;
            const auto deltas = prepareOblivionCounterContactDeltas(world, actionId, attacker, *contact);
            if (!deltas) return std::nullopt;
            OblivionOrdinaryContactResult result{*contact};
            if (!contact->isEmpty())
            {
                auto& inventory = attacker.getClass().getInventoryStore(attacker);
                if (const auto left = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedLeft);
                    left != inventory.end() && left->getType() == ESM::REC_ARMO)
                {
                    const auto id = MWWorld::OblivionProfileServices::nativeItemId(world.getStore(), left->getCellRef().getRefId());
                    const auto* form = id.getIf<ESM::FormId>();
                    const auto* shield = form ? world.getStore().get<ESM4::Armor>().search(*form) : nullptr;
                    if (!shield || !(shield->mArmorFlags & ESM4::Armor::TES4_Shield)) return std::nullopt;
                    result.mHitSounds = ESM4::nativeNpcMeleeHitSounds({-1, -1,
                        (shield->mGeneralFlags & ESM4::Armor::TES4_HeavyArmor) ? 1 : 0});
                }
            }
            auto prepared = nativeWorld->prepareOblivionPhysicalContact(actionId, attacker, *contact, *deltas);
            if (!prepared || !prepared->commit()) return std::nullopt;
            result.mRandomDraws = prepared->randomDraws();
            dispatchCommittedMeleeHit(*nativeWorld, attacker, *contact);
            return result;
        }
        // Directional mastery contacts use the owned Player/NPC phase path.
        if (kind > ESM4::MeleeStrikeKind::StandingPower
            && kind != ESM4::MeleeStrikeKind::LeftPower && kind != ESM4::MeleeStrikeKind::RightPower
            && kind != ESM4::MeleeStrikeKind::ForwardPower && kind != ESM4::MeleeStrikeKind::BackwardPower)
            return std::nullopt;
        const bool power = kind >= ESM4::MeleeStrikeKind::StandingPower;
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
        const auto playerContact = world.getPlayerPtr();
        const bool longitudinalPower = kind == ESM4::MeleeStrikeKind::ForwardPower
            || kind == ESM4::MeleeStrikeKind::BackwardPower;
        const bool directionalPowerPair = longitudinalPower && !contact->isEmpty()
            && ((attacker == playerContact && contact->getType() == ESM::REC_NPC_4)
                || (attacker.getType() == ESM::REC_NPC_4 && *contact == playerContact));
        if (longitudinalPower && !contact->isEmpty() && !directionalPowerPair)
            return std::nullopt; // Remaining actor routes need the same complete effect owner.
        OblivionOrdinaryContactResult result{*contact, {0, 0}};
        std::int32_t soundWeaponType = -1;
        std::int32_t hitArmorClass = -1;
        float preArmorDamage = 0;
        bool blockAccepted = false;
        bool requestsAttackerRecoil = false;
        bool weaponBlockSound = false;
        bool enchantedSound = false;
        bool observeEnchantment = false;
        float physicalHealth = 0, enchantmentHealth = 0;
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
                result.mImmune = damage->mImmune;
                result.mDamage = {damage->mHealth, damage->mFatigue};
                result.mBlockFatigueDebit = damage->mBlockFatigueDebit;
                result.mBlockAbsorbedFraction = damage->mBlockAbsorbedFraction;
                deltas.mOrdinaryHitArguments = damage->mHitArguments;
                blockAccepted = damage->mBlockAccepted;
                requestsAttackerRecoil = damage->mRequestsAttackerRecoil;
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
                result.mImmune = damage->mImmune;
                result.mDamage = {damage->mHealth + damage->mEnchantmentHealth, 0};
                observeEnchantment = !weapon->mEnchantment.isZeroOrUnset();
                physicalHealth = damage->mHealth; enchantmentHealth = damage->mEnchantmentHealth;
                if (damage->mEnchantmentChargeChange)
                    deltas.mEnchantmentChargeChanges.push_back(*damage->mEnchantmentChargeChange);
                enchantedSound = damage->mEnchantedSound;
                result.mBlockFatigueDebit = damage->mBlockFatigueDebit;
                result.mBlockAbsorbedFraction = damage->mBlockAbsorbedFraction;
                deltas.mOrdinaryHitArguments = damage->mHitArguments;
                blockAccepted = damage->mBlockAccepted;
                requestsAttackerRecoil = damage->mRequestsAttackerRecoil;
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
        if (result.mImmune)
        {
            // Original5FF187..5FF1B7 exits before damage, wear, hit sounds,
            // reactions and hit speech. Attack expenditure still belongs to
            // the caller's once-only contact window, including power attacks.
            const bool notify = attacker == playerContact;
            auto* windows = MWBase::Environment::get().getWindowManagerOrNull();
            if (notify && !windows) return std::nullopt;
            std::string message = "Your attack has no effect.";
            if (notify)
                for (const auto* setting : settings)
                    if (Misc::StringUtils::ciEqual(setting->mEditorId, "sNormalWeaponsResisted"))
                    {
                        const auto* text = std::get_if<std::string>(&setting->mData);
                        if (!text) throw std::invalid_argument("incorrect native immunity message type");
                        message = *text;
                        break;
                    }
            const OblivionPhysicalContactDeltas immune{-cost, 0};
            auto prepared = nativeWorld->prepareOblivionPhysicalContact(actionId, attacker, *contact, immune);
            if (!prepared || !prepared->commit()) return std::nullopt;
            result.mHitSounds = std::array<std::string_view, 3>{};
            const auto victim = *contact == playerContact ? ESM::FormKey::dynamic("player", 1)
                : contact->getCellRef().getFormKey();
            Log(Debug::Verbose) << "M15 immune melee contact committed: actor=" << actor.serialize()
                << " id=" << actionId << " victim=" << victim.serialize()
                << " notified=" << notify;
            if (notify) windows->messageBox(message);
            return result;
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
        if (!contact->isEmpty() && (*contact == world.getPlayerPtr() || contact->getType() == ESM::REC_NPC_4
                || contact->getType() == ESM::REC_CREA4)
            && !result.mHitSounds)
        {
            // Original5FFB05 re-reads the shield after blocking wear. With
            // neither weapon-block nor shield sound,5FFB96..5FFD61 uses the
            // general hit layers, including unarmed blocks and removed shields.
            // Original5FFCE5 uses the armor selected by this contact's wear
            // path;5FFD61 uses no material even if other armor is equipped.
            // Model initialization projects Creature Actor byte104 as
            // canonical0/1; the TESCreature base type is not this sound input.
            result.mHitSounds = ESM4::nativeMeleeHitSounds({soundWeaponType, hitArmorClass, -1, false, enchantedSound},
                contact->getType() == ESM::REC_CREA4
                    ? std::optional<std::uint8_t>{contact->getRefData().getOblivionCanSpendFatigue() ? 1u : 0u}
                    : std::nullopt);
        }
        deltas.mVictimFatigue = -result.mDamage.mFatigue;
        deltas.mVictimBlockFatigue = -result.mBlockFatigueDebit;
        // NPC Hit responses observe the old friend-hit count; notification
        // follows reactions. The stationary enemy path owns both stages.
        // Low-Hand weapon blocks preserve full damage and defer the recoil
        // chance to the existing post-damage general-hit tail. These flags
        // were captured together by the numeric block preparation above.
        const bool generalHitBlock = blockAccepted && deltas.mOrdinaryHitArguments
            && deltas.mOrdinaryHitArguments->mActiveBlock
            && deltas.mOrdinaryHitArguments->mBareHandBlock
            && deltas.mOrdinaryHitArguments->mAttackerHasWeapon && !requestsAttackerRecoil;
        const bool playerNpcPair = !contact->isEmpty()
            && ((attacker == playerContact && contact->getType() == ESM::REC_NPC_4)
                || (attacker.getType() == ESM::REC_NPC_4 && *contact == playerContact));
        if (generalHitBlock && !playerNpcPair)
            return std::nullopt; // The complete NPC/NPC general-hit owner remains separate.
        const bool phased = !contact->isEmpty() && deltas.mOrdinaryHitArguments
            && (generalHitBlock || directionalPowerPair || (attacker != playerContact && attacker.getType() == ESM::REC_NPC_4
                    && *contact == playerContact)
                || nativeWorld->supportsOblivionEnemyHitNotification(attacker, *contact));
        // Original600528 blocked dispatch bypasses5FFF99's general-hit force,
        // including its advanced unarmed zero-absorption route.
        if (!phased && !contact->isEmpty() && (contact->getType() == ESM::REC_NPC_4 || contact->getType() == ESM::REC_CREA4)
            && result.mBlockAbsorbedFraction == 0 && !requestsAttackerRecoil
            && oblivionKnockedState(world, *contact) == 0
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
            && (*contact == world.getPlayerPtr() || contact->getType() == ESM::REC_NPC_4
                || contact->getType() == ESM::REC_CREA4);
        // Original600528..60056A requests nonprojectile Recoil after a
        // Counter miss even when earlier wear removed the incoming weapon.
        // Resource discovery uses the projected empty Right slot in that case.
        // Advanced unarmed weapon blocks also reach that dispatcher at zero
        // absorption. Preparation keeps this decision separate from damage.
        const bool attackerRecoil = blockHit && requestsAttackerRecoil
            && (attacker == world.getPlayerPtr() || attacker.getType() == ESM::REC_NPC_4
                || attacker.getType() == ESM::REC_CREA4);
        const auto observeCommittedEnchantment = [&] {
            if (!observeEnchantment) return;
            const bool spent = !deltas.mEnchantmentChargeChanges.empty();
            Log(Debug::Verbose) << "M15 weapon enchantment contact: actor=" << actor.serialize()
                << " id=" << std::dec << actionId << " physical_health_damage=" << physicalHealth
                << " enchantment_health_damage=" << enchantmentHealth
                << " total_health_damage=" << result.mDamage.mHealth
                << " charge_written=" << static_cast<unsigned>(spent)
                << " charge_after=" << (spent ? deltas.mEnchantmentChargeChanges.front().mCharge : 0.f);
        };
        if (phased)
        {
            const auto draws = nativeWorld->commitOblivionPlayerNpcHit(actionId, attacker, *contact, deltas, [&] {
                emitOblivionMeleeHitSounds(world, actionId, attacker, result);
                result.mHitSoundsEmitted = true;
            });
            if (!draws) return std::nullopt;
            result.mRandomDraws = *draws;
            observeCommittedEnchantment();
            // All contact continuations have published before a script can
            // Kill, change a quest, or save. The consumed strike owns once-only
            // dispatch, including after restore; misses have no struck script.
            dispatchCommittedMeleeHit(*nativeWorld, attacker, *contact);
            return result;
        }
        auto prepared = nativeWorld->prepareOblivionPhysicalContact(
            actionId, attacker, *contact, deltas, blockHit, attackerRecoil);
        if (!prepared || !prepared->commit())
            return std::nullopt;
        result.mRandomDraws = prepared->randomDraws();
        observeCommittedEnchantment();
        dispatchCommittedMeleeHit(*nativeWorld, attacker, *contact);
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
        bool creatureHasUnresolvedContactSpells(const MWWorld::ESMStore& store,
            const OblivionCombatService& service, const MWWorld::Ptr& actor)
        {
            if (actor.isEmpty() || actor.getType() != ESM::REC_CREA4)
                return false;
            const auto* values = service.findActorValues(actor.getCellRef().getFormKey());
            if (!values) return true;
            const auto sources = MWWorld::resolveOblivionCreatureContactSpellSources(store, values->mBase);
            if (!sources) return true;
            const auto abilities = MWWorld::resolveOblivionCreaturePassiveAbilityInputs(store, values->mBase);
            if (!abilities) return true;
            for (const auto& ability : *abilities)
            {
                if (!values->mPassiveAbilities) return true;
                const auto owned = std::find_if(values->mPassiveAbilities->begin(), values->mPassiveAbilities->end(),
                    [&](const auto& entry) { return entry.mSpell == ability.mSpell; });
                if (owned == values->mPassiveAbilities->end() || owned->mSpellType != 4
                    || owned->mEffects.size() != ability.mEffects.size()) return true;
                for (std::size_t i = 0; i < ability.mEffects.size(); ++i)
                {
                    const auto& expected = ability.mEffects[i];
                    const auto& applied = owned->mEffects[i];
                    if (applied.mEffectIndex != expected.mEffectIndex || applied.mCode != expected.mCode
                        || applied.mActorValue != expected.mValues.mActorValue
                        || applied.mInitialMagnitude != expected.mValues.mMagnitude) return true;
                    const float signedMagnitude = expected.mFlags & 4
                        ? -expected.mValues.mMagnitude : expected.mValues.mMagnitude;
                    if (!ESM4::valueModifierRequiresCurrent(static_cast<std::uint8_t>(applied.mActorValue),
                            applied.mCode, signedMagnitude) && applied.mStoredMagnitude != signedMagnitude)
                        return true;
                }
            }
            if (sources->empty()) return false; // No leveled contact spell requires a selection snapshot.
            if (!values->mLeveledContactSpells) return true; // Unknown legacy selection must not reroll here.
            return std::any_of(values->mLeveledContactSpells->begin(), values->mLeveledContactSpells->end(),
                [&](const auto& selected) {
                    return std::none_of(sources->begin(), sources->end(), [&](const auto& source) {
                        return source.mList->mEntries[0].mSpellKey == selected;
                    });
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
            bool mActiveBlock = false;
            bool mBareHandBlock = false;
            bool mWeaponBlockSound = false;
            bool mRequestsAttackerRecoil = false;
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
            float remaining, std::int32_t attackerWeaponType, std::span<const ESM4::GameSetting* const> settings,
            bool enchantedSound = false)
        {
            PreparedBlock result{remaining};
            if (!oblivionBlockingPosture(world, victim) || oblivionParalyzed(world, victim))
                return result;
            // Original5FF7E6/5FF81D cache these before the facing gate.
            // Read presence first: definitions and supported mitigation policy
            // are needed only after facing accepts the contact.
            auto& inventory = victim.getClass().getInventoryStore(victim);
            MWWorld::Ptr item;
            bool hasShield = false;
            if (auto left = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedLeft);
                left != inventory.end() && left->getType() == ESM::REC_ARMO)
            {
                item = *left;
                hasShield = true;
            }
            else if (auto right = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
                right != inventory.end() && right->getType() == ESM::REC_WEAP)
                item = *right;
            result.mActiveBlock = true;
            result.mBareHandBlock = item.isEmpty();
            const auto& from = attacker.getRefData().getPosition();
            const auto& to = victim.getRefData().getPosition();
            const float dx = static_cast<float>(double(from.pos[0]) - to.pos[0]);
            const float dy = static_cast<float>(double(from.pos[1]) - to.pos[1]);
            if (!ESM4::combatHitCone(to.rot[2], std::atan2(dx, dy),
                    ESM4::buildCombatHitConeAngle(settings)).mInside)
                return result;
            const ESM4::Armor* shield = nullptr;
            const ESM4::Weapon* weapon = nullptr;
            if (!item.isEmpty())
            {
                const auto id = MWWorld::OblivionProfileServices::nativeItemId(world.getStore(), item.getCellRef().getRefId());
                const auto* form = id.getIf<ESM::FormId>();
                if (hasShield)
                {
                    shield = form ? world.getStore().get<ESM4::Armor>().search(*form) : nullptr;
                    if (!shield || !(shield->mArmorFlags & ESM4::Armor::TES4_Shield))
                        throw std::invalid_argument("native blocking shield has no winning shield definition");
                }
                else
                {
                    weapon = form ? world.getStore().get<ESM4::Weapon>().search(*form) : nullptr;
                    if (!weapon || weapon->mData.type > 3)
                        return std::nullopt;
                }
            }
            const auto equipment = shield ? ESM4::BlockEquipment::Shield
                : weapon ? ESM4::BlockEquipment::Weapon : ESM4::BlockEquipment::Unarmed;
            const auto disposition = ESM4::blockContactDisposition(
                {true, false, false, true, equipment, attackerWeaponType >= 0, false});
            const bool player = victim == world.getPlayerPtr();
            const auto key = player ? ESM::FormKey::dynamic("player", 1) : victim.getCellRef().getFormKey();
            const auto integer = [&](std::uint8_t av) { return player ? service.getPlayerIntegerValue(av)
                : service.getNonPlayerIntegerValue(victim, av); };
            const auto base = [&](std::uint8_t av) { return player ? service.getPlayerBaseValue(av)
                : service.getNonPlayerBaseValue(key, av, world.getStore()); };
            const bool reactionOnly = disposition == ESM4::BlockContactDisposition::ReactionOnly;
            const bool generalHitBlock = reactionOnly
                && ESM4::combatMastery(base(17), ESM4::buildCombatMasterySettings(settings))
                    < ESM4::CombatMastery::Journeyman;
            // Counter-first selection and immediate Master disarm join the
            // World contact after this numeric preparation and armor-wear RNG.
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
                    (shield->mGeneralFlags & ESM4::Armor::TES4_HeavyArmor) ? 1 : 0, false, enchantedSound});
            else if (weapon)
            {
                result.mWeaponBlockSound = true;
                const auto layers = ESM4::nativeMeleeBlockSounds(attackerWeaponType,
                    static_cast<std::int32_t>(weapon->mData.type), enchantedSound);
                result.mSounds = std::array<std::string_view, 3>{layers[0], layers[1], {}};
            }
            else if (ESM4::combatMastery(base(15), ESM4::buildCombatMasterySettings(settings))
                >= ESM4::CombatMastery::Journeyman)
            {
                // Original5FFA21..5FFAFB selects the unarmed block pair at
                // Journeyman Block, independently of the incoming weapon type.
                result.mWeaponBlockSound = true;
                const auto layers = ESM4::nativeMeleeBlockSounds(-1, -1);
                result.mSounds = std::array<std::string_view, 3>{layers[0], layers[1], {}};
            }
            result.mFraction = reactionOnly ? 0.f : ESM4::blockFraction({integer(15), integer(7),
                ESM4::combatFatigueRatio(fatigue, base(10)), equipment},
                ESM4::buildBlockSettings(settings), ESM4::buildPhysicalCombatSettings(settings));
            result.mRequestsAttackerRecoil = (reactionOnly && !generalHitBlock) || result.mFraction > 0;
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
                            const auto layers = ESM4::nativeMeleeBlockSounds(attackerWeaponType, -1, enchantedSound);
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
        const auto resistance = integer(victim, 65);
        if (sneaking)
            return std::nullopt;
        if (kind > ESM4::MeleeStrikeKind::StandingPower
            && kind != ESM4::MeleeStrikeKind::LeftPower && kind != ESM4::MeleeStrikeKind::RightPower
            && kind != ESM4::MeleeStrikeKind::ForwardPower && kind != ESM4::MeleeStrikeKind::BackwardPower)
            return std::nullopt;
        auto incoming = creature ? oblivionCreatureNaturalContactDamage(world, attacker)
            : oblivionHandToHandContactDamage(world, attacker, victim);
        if (creatureHasUnresolvedContactSpells(world.getStore(), *service, attacker)
            || creatureHasUnresolvedContactSpells(world.getStore(), *service, victim))
            return std::nullopt; // Creature passive/contact effects must join this transaction.
        if (creature && !creatureUsesDefaultWeaponHitSound(world.getStore(), attacker))
            return std::nullopt;
        if (creature && incoming.mHealth < 0)
            return std::nullopt; // Signed negative source damage remains outside the admitted sink domain.
        // Zero natural damage still owns a real contact, attack expenditure,
        // block/reaction and sound. The sink skips its zero Health/Fatigue writes.
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& record : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(record.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(record.mId));
        if (resistance >= 100)
        {
            const auto hand = attacker == player ? service->getPlayerBaseValue(17)
                : service->getNonPlayerBaseValue(attacker.getCellRef().getFormKey(), 17, world.getStore());
            if (ESM4::combatMastery(hand, ESM4::buildCombatMasterySettings(settings))
                < ESM4::CombatMastery::Journeyman)
            {
                OblivionUnarmedContactDamage immune{0, 0};
                immune.mImmune = true;
                return immune;
            }
        }
        // Original5FF576 skips partial reduction when both weapon and
        // projectile are null, including Creature natural contacts. Health
        // and Fatigue stay unchanged; base HTH mastery (Creature Combat group)
        // at Journeyman also bypasses the earlier full-immunity branch.
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
            block->mAccepted, block->mWeaponBlockSound, block->mSounds, wear.mHitArmorClass,
            block->mRequestsAttackerRecoil,
            // Creature natural damage bypasses native5FF423; its later cached
            // skill query is not established. 255 marks this field unavailable.
            {block->mRemaining, damage.mFatigue, incoming.mHealth, block->mFraction,
                static_cast<std::uint8_t>(creature ? 255 : 17),
                block->mRemaining > 0 || incoming.mFatigue > 0, block->mActiveBlock, block->mBareHandBlock, false}};
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

    namespace
    {
        struct PreparedWeaponEnchantment
        {
            float mHealth = 0;
            std::optional<OblivionPhysicalEnchantmentChargeChange> mCharge;
            bool mActive = false;
        };

        std::optional<PreparedWeaponEnchantment> prepareWeaponEnchantment(MWBase::World& world,
            OblivionCombatService& service, const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim,
            const MWWorld::Ptr& item, const ESM4::Weapon& weapon, float difficulty,
            std::span<const ESM4::GameSetting* const> settings)
        {
            if (weapon.mEnchantment.isZeroOrUnset()) return PreparedWeaponEnchantment{};
            const auto& store = world.getStore();
            const auto* enchantment = store.get<ESM4::Enchantment>().search(weapon.mEnchantment);
            if (!enchantment || !enchantment->mData || enchantment->mData->mType != 2
                || (enchantment->mData->mFlags & ~1u)) return std::nullopt;
            const auto definition = [&](std::uint32_t code) -> const ESM4::EffectSetting* {
                const ESM4::EffectSetting* result = nullptr;
                std::set<ESM::FormId> seen;
                for (const auto& entry : store.get<ESM4::EffectSetting>())
                    if (seen.insert(entry.mId).second)
                        if (const auto* effect = store.get<ESM4::EffectSetting>().search(entry.mId);
                            effect && effect->mEffectCode == code)
                        {
                            if (result) return nullptr;
                            result = effect;
                        }
                return result;
            };
            std::vector<ESM4::WeaponEnchantmentCostEffect> costs;
            const bool manual = enchantment->mData->mFlags & 1;
            if (!manual)
                for (const auto& effect : enchantment->mEffects)
                {
                    const auto* setting = definition(effect.mId);
                    if (!setting || !setting->mData || effect.mScriptEffect) return std::nullopt;
                    const auto flags = setting->mData->mFlags;
                    costs.push_back({setting->mData->mBaseCost, effect.mMagnitude, effect.mArea,
                        effect.mDuration, effect.mRange, bool(flags & 0x100), bool(flags & 0x200),
                        bool(flags & 0x80), bool(flags & 0x400000)});
                }
            const auto cost = ESM4::weaponEnchantmentCost(manual
                    ? std::optional(enchantment->mData->mCost) : std::nullopt,
                costs, ESM4::buildWeaponEnchantmentCostSettings(settings));
            const auto charge = ESM4::weaponEnchantmentCharge(item.getCellRef().getEnchantmentCharge(),
                weapon.mEnchantmentPoints, cost);
            if (!charge.mAvailable) return PreparedWeaponEnchantment{};
            // Narrow S4 contact effect. Do not silently treat a script, timed,
            // area or different effect as executed. General magic stays open.
            if (enchantment->mEffects.size() != 1) return std::nullopt;
            const auto& effect = enchantment->mEffects.front();
            const auto* setting = definition(effect.mId);
            if (effect.mId != ESM::fourCC("FIDG") || effect.mScriptEffect || effect.mRange != 1
                || effect.mArea != 0 || effect.mDuration != 0 || effect.mActorValue != 8
                || !setting || !setting->mData || setting->mData->mAssociatedData != 8
                || setting->mData->mResistanceActorValue != 61) return std::nullopt;
            const auto player = world.getPlayerPtr();
            const auto value = [&](std::uint8_t av) {
                return victim == player ? service.getPlayerIntegerValue(av)
                    : service.getNonPlayerIntegerValue(victim, av);
            };
            // These branches need their real effect/caster decisions; admitting
            // the unresisted profile must not erase absorption or reflection.
            if (value(61) != 0 || value(64) != 0 || value(52) != 0 || value(53) != 0)
                return std::nullopt;
            const auto role = victim == player ? ESM4::PlayerDamageRole::Victim
                : attacker == player ? ESM4::PlayerDamageRole::Attacker : ESM4::PlayerDamageRole::Unaffected;
            PreparedWeaponEnchantment result;
            result.mHealth = ESM4::difficultyDamage(static_cast<float>(effect.mMagnitude), difficulty,
                ESM4::buildDifficultyDamageMultiplier(settings), role);
            result.mCharge = captureOblivionPhysicalEnchantmentChargeChange(attacker, item, *charge.mAfter);
            result.mActive = true;
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
        const bool creatureAttacker = !attacker.isEmpty() && attacker.getType() == ESM::REC_CREA4;
        if (!service || (!eligible(attacker) && !creatureAttacker) || (!eligible(victim) && !creatureVictim)
            || attacker == victim || sneaking)
            return std::nullopt;
        // Validate authority before class queries can initialize shared caches.
        if (victim == player) (void)service->getPlayerValue(8);
        else (void)service->getNonPlayerValue(victim, 8);
        const auto resistance = victim == player ? service->getPlayerIntegerValue(65)
            : service->getNonPlayerIntegerValue(victim, 65);
        if (kind > ESM4::MeleeStrikeKind::StandingPower
            && kind != ESM4::MeleeStrikeKind::LeftPower && kind != ESM4::MeleeStrikeKind::RightPower
            && kind != ESM4::MeleeStrikeKind::ForwardPower && kind != ESM4::MeleeStrikeKind::BackwardPower)
            return std::nullopt;
        const float rawIncoming = weaponContactDamage(world, attacker, item, kind);
        if (creatureHasUnresolvedContactSpells(world.getStore(), *service, attacker)
            || creatureHasUnresolvedContactSpells(world.getStore(), *service, victim))
            return std::nullopt; // Do not omit Creature passive/contact-effect consequences.
        if (rawIncoming < 0)
            return std::nullopt; // Signed bonus/nonpositive sink policy needs its own branch.
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& record : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(record.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(record.mId));
        const auto id = MWWorld::OblivionProfileServices::nativeItemId(world.getStore(), item.getCellRef().getRefId());
        const auto* weapon = world.getStore().get<ESM4::Weapon>().search(*id.getIf<ESM::FormId>());
        const auto enchantment = prepareWeaponEnchantment(world, *service, attacker, victim, item,
            *weapon, normalizedDifficulty, settings);
        if (!enchantment) return std::nullopt;
        // Original5FF108 uses WEAP DATA bit0 (silver).5E46C0 returns true
        // even after insufficient charge clears the selected ENCH effect:
        // depletion suppresses magic damage, not the physical bypass.
        const auto resisted = ESM4::resistNormalWeapon(rawIncoming, resistance,
            (weapon->mData.flags & 1u) || !weapon->mEnchantment.isZeroOrUnset());
        if (resisted.mImmune)
        {
            OblivionWeaponContactDamage immune{};
            immune.mImmune = true;
            return immune;
        }
        const float incoming = resisted.mHealthDamage;
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
            armor.mHealthDamage, static_cast<std::int32_t>(weapon->mData.type), settings, enchantment->mActive);
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
        auto result = OblivionWeaponContactDamage{damage.mHealth, condition, std::move(wear->mConditions), wear->mRandom,
            incoming, block->mDebit, block->mFraction, block->mAccepted, block->mWeaponBlockSound, block->mSounds,
            wear->mHitArmorClass, breakDisposition, block->mRequestsAttackerRecoil,
            {block->mRemaining, 0, incoming, block->mFraction,
                static_cast<std::uint8_t>(weapon->mData.type < 2 ? 14 : 16),
                block->mRemaining > 0, block->mActiveBlock, block->mBareHandBlock, true}};
        result.mEnchantmentHealth = enchantment->mHealth;
        result.mEnchantmentChargeChange = enchantment->mCharge;
        result.mEnchantedSound = enchantment->mActive;
        return result;
    }

    std::optional<OblivionWeaponContactDamage> resolveOblivionArrowContact(
        MWBase::World& world, const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim,
        float launchDamage, float normalizedDifficulty)
    {
        auto* native = dynamic_cast<MWWorld::World*>(&world);
        auto* service = native ? native->getOblivionCombatService() : nullptr;
        if (!service || !std::isfinite(launchDamage) || launchDamage < 0)
            throw std::invalid_argument("native arrow contact requires finite launch damage and authority");
        const auto player = world.getPlayerPtr();
        const auto resistance = victim == player ? service->getPlayerValue(65) : service->getNonPlayerValue(victim, 65);
        const auto resisted = ESM4::resistNormalWeapon(launchDamage, resistance, false);
        OblivionWeaponContactDamage result{};
        if (resisted.mImmune) { result.mImmune = true; return result; }
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& record : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(record.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(record.mId));
        const float incoming = resisted.mHealthDamage;
        const auto armor = ESM4::mitigateArmor(incoming, oblivionArmorRating(world, victim),
            ESM4::buildArmorRatingSettings(settings).mSkillMaximum, false);
        const auto block = prepareOrdinaryBlock(world, *service, attacker, victim, armor.mHealthDamage, 5, settings, false);
        if (!block) return {};
        const auto role = victim == player ? ESM4::PlayerDamageRole::Victim
            : attacker == player ? ESM4::PlayerDamageRole::Attacker : ESM4::PlayerDamageRole::Unaffected;
        const auto damage = ESM4::physicalContactDamage({incoming, 0}, block->mRemaining,
            normalizedDifficulty, ESM4::buildDifficultyDamageMultiplier(settings), role);
        auto wear = prepareOrdinaryArmorWear(world, *service, attacker, victim, incoming, armor.mAbsorbedFraction,
            settings, block->mConditions, block->mAccepted ? std::optional<float>{ESM4::blockedArmorWear(
                ESM4::armorWear(incoming, armor.mAbsorbedFraction, ESM4::buildDurabilitySettings(settings)),
                block->mFraction, block->mUnarmed)} : std::nullopt);
        if (!wear) return {};
        result.mHealth = damage.mHealth;
        result.mVictimConditionChanges = std::move(wear->mConditions);
        result.mRandomTransition = wear->mRandom;
        result.mPreArmorDamage = incoming;
        result.mBlockFatigueDebit = block->mDebit;
        result.mBlockAbsorbedFraction = block->mFraction;
        result.mBlockAccepted = block->mAccepted;
        result.mHitArmorClass = wear->mHitArmorClass;
        return result;
    }

    static float weaponContactDamage(MWBase::World& world,
        const MWWorld::Ptr& attacker, const MWWorld::Ptr& item, ESM4::MeleeStrikeKind kind)
    {
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        const auto player = world.getPlayerPtr();
        if (!service || attacker.isEmpty()
            || (attacker != player && attacker.getType() != ESM::REC_NPC_4 && attacker.getType() != ESM::REC_CREA4)
            || item.isEmpty() || item.getType() != ESM::REC_WEAP)
            throw std::invalid_argument("native ordinary weapon query requires a native actor and equipped weapon");
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
            // Original5FF4B4..5FF4F1 skips the power multiplier for CREA.
            attacker.getType() == ESM::REC_CREA4 ? 1.f
                : contactPowerMultiplier(world, *service, attacker, weapon->mData.type < 2 ? 14 : 16, kind, settings)},
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

    float oblivionInventoryWeight(MWBase::World& world, const MWWorld::Ptr& actor)
    {
        if (world.getGameProfile() != ESM::GameProfile::Oblivion || actor.isEmpty())
            throw std::invalid_argument("native inventory weight requires a native actor");
        auto* nativeWorld = dynamic_cast<MWWorld::World*>(&world);
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        const bool player = actor == world.getPlayerPtr();
        const auto key = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
        const bool hasValues = service && service->findActorValues(key);
        auto& inventory = actor.getClass().getInventoryStore(actor);
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        if (hasValues)
            for (const auto& record : world.getStore().get<ESM4::GameSetting>())
                if (seen.insert(record.mId).second)
                    settings.push_back(world.getStore().get<ESM4::GameSetting>().search(record.mId));
        const auto mastery = ESM4::buildCombatMasterySettings(settings);
        const auto armorMastery = ESM4::buildArmorMasterySettings(settings);
        double result = 0;
        for (auto iterator = inventory.begin(); iterator != inventory.end(); ++iterator)
        {
            const MWWorld::Ptr item = *iterator;
            const auto count = item.getCellRef().getCount();
            if (count <= 0) continue;
            const float weight = item.getClass().getWeight(item);
            result += double(weight) * count;
            if (!hasValues || item.getType() != ESM::REC_ARMO || !inventory.isEquipped(item)) continue;
            const auto id = MWWorld::OblivionProfileServices::nativeItemId(
                world.getStore(), item.getCellRef().getRefId());
            const auto* armor = world.getStore().get<ESM4::Armor>().search(id);
            if (!armor) continue;
            const bool heavy = armor->mGeneralFlags & ESM4::Armor::TES4_HeavyArmor;
            const auto skill = player ? service->getPlayerBaseValue(heavy ? 18 : 27)
                : service->getNonPlayerBaseValue(key, heavy ? 18 : 27, world.getStore());
            // Only the worn instance receives the perk, never spare copies or
            // the stored item weight. Recompute from live slots after unequip/load.
            result += ESM4::wornArmorWeight(weight, heavy, skill, true, armorMastery, mastery) - weight;
        }
        return std::max(0.f, static_cast<float>(result));
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
