#include "oblivionmelee.hpp"

#include "oblivioncombat.hpp"
#include "oblivionai.hpp"
#include "creaturestats.hpp"
#include "movement.hpp"
#include "../mwworld/oblivioncombatdata.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwphysics/actor.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/oblivionprofileservices.hpp"
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadweap.hpp>
#include "../mwworld/worldimp.hpp"
#include <components/esm/records.hpp>
#include <components/esm4/combatsettings.hpp>
#include <components/misc/strings/algorithm.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

namespace MWMechanics
{
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
        if (!combat || !ai || actor.isEmpty() || actor.getType() != ESM::REC_NPC_4)
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
        float reach = ESM4::unarmedMeleeReach(scale.z(), reachSettings);
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
        stats.setAttackingOrSpell(canReachOblivionMeleeTarget(world, actor, target, reach));
        return true;
    }

    std::optional<OblivionOrdinaryContactResult> commitOblivionOrdinaryMeleeContact(
        MWBase::World& world, std::uint64_t actionId,
        const MWWorld::Ptr& attacker, const MWWorld::Ptr& selectedTarget,
        float reach, float weaponWeight, float normalizedDifficulty, bool sneaking)
    {
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        if (!service || attacker.isEmpty() || attacker.getType() == ESM::REC_CREA4)
            return std::nullopt; // Creature process/fatigue eligibility is a separate caller.
        const auto actor = attacker == world.getPlayerPtr() ? ESM::FormKey::dynamic("player", 1)
            : attacker.getCellRef().getFormKey();
        if (!service->isOrdinaryMeleeContactPending(actionId, actor))
            return std::nullopt;
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& record : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(record.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(record.mId));
        const float cost = ESM4::attackFatigueCost(weaponWeight, false,
            ESM4::buildAttackFatigueSettings(settings));
        const auto contact = acquireOblivionMeleeContact(world, actionId, attacker, selectedTarget, reach);
        if (!contact)
            return std::nullopt;
        OblivionOrdinaryContactResult result{*contact, {0, 0}};
        std::int32_t soundWeaponType = -1;
        OblivionPhysicalContactDeltas deltas{-cost, 0, 0, 0};
        if (!contact->isEmpty())
        {
            const auto* state = service->findMeleeState(actor);
            if (!state || !state->mStrike)
                return std::nullopt;
            if (state->mStrike->mWeaponBase.isNull())
            {
                const auto damage = resolveOblivionOrdinaryUnarmedContact(world, attacker,
                    *contact, normalizedDifficulty, sneaking);
                if (!damage)
                    return std::nullopt;
                result.mDamage = {damage->mHealth, damage->mFatigue};
                result.mBlockFatigueDebit = damage->mBlockFatigueDebit;
                result.mBlockAbsorbedFraction = damage->mBlockAbsorbedFraction;
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
                const auto damage = resolveOblivionOrdinaryWeaponContact(world, attacker,
                    *contact, item, normalizedDifficulty, sneaking);
                if (!damage)
                    return std::nullopt;
                result.mDamage = {damage->mHealth, 0};
                result.mWeaponConditionAfterWear = damage->mConditionAfterWear;
                result.mArmorConditionWrites = static_cast<unsigned>(damage->mArmorConditionChanges.size());
                result.mRandomDraws = damage->mRandomTransition ? damage->mRandomTransition->mDraws : 0;
                if (damage->mConditionAfterWear)
                {
                    condition.mCondition = *damage->mConditionAfterWear;
                    deltas.mConditionChanges.push_back(std::move(condition));
                }
                deltas.mConditionChanges.insert(deltas.mConditionChanges.end(),
                    damage->mArmorConditionChanges.begin(), damage->mArmorConditionChanges.end());
                deltas.mRandomTransition = damage->mRandomTransition;
            }
        }
        deltas.mVictimHealth = -result.mDamage.mHealth;
        if (!contact->isEmpty() && (*contact == world.getPlayerPtr() || contact->getType() == ESM::REC_NPC_4)
            && !oblivionBlockingPosture(world, *contact))
        {
            // Unarmored ordinary NPC contacts reach original5FFD61 with no
            // armor/shield material and neither selector flag. Armor-hit
            // selection and Creature sound families remain separate adapters.
            auto& inventory = contact->getClass().getInventoryStore(*contact);
            const bool armored = std::any_of(inventory.begin(), inventory.end(), [&](const auto& item) {
                return item.getType() == ESM::REC_ARMO && inventory.isEquipped(item);
            });
            if (!armored)
                result.mHitSounds = ESM4::nativeNpcMeleeHitSounds({soundWeaponType});
        }
        deltas.mVictimFatigue = -result.mDamage.mFatigue;
        deltas.mVictimBlockFatigue = -result.mBlockFatigueDebit;
        if (!world.commitOblivionPhysicalContact(actionId, attacker, *contact, deltas))
            return std::nullopt;
        return result;
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

    float oblivionNormalizedDifficulty(int difficultySetting)
    {
        return static_cast<float>(double(std::clamp(difficultySetting, -100, 100)) / 100.0);
    }

    std::optional<OblivionUnarmedContactDamage> resolveOblivionOrdinaryUnarmedContact(
        MWBase::World& world, const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim,
        float normalizedDifficulty, bool sneaking)
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
        if (!service || !eligible(attacker) || !eligible(victim))
            return std::nullopt; // Creature Fatigue eligibility is not NPC eligibility.
        const auto integer = [&](const MWWorld::Ptr& ptr, std::uint8_t av) {
            return ptr == player ? service->getPlayerIntegerValue(av)
                : service->getNonPlayerIntegerValue(ptr, av);
        };
        // These branches require sneak/mastery, block or gear-wear policy.
        // Do not substitute a generic TES3 hit or silently omit their effects.
        if (sneaking || integer(victim, 65) != 0)
            return std::nullopt;
        const auto incoming = oblivionHandToHandContactDamage(world, attacker, victim);
        if (incoming.mFatigue <= 0)
            return std::nullopt; // Native zero-Fatigue contacts enter armor wear.
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& record : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(record.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(record.mId));
        const auto armor = ESM4::mitigateArmor(incoming.mHealth, oblivionArmorRating(world, victim),
            ESM4::buildArmorRatingSettings(settings).mSkillMaximum, false);
        float remaining = armor.mHealthDamage;
        float blockFraction = 0, blockDebit = 0;
        if (oblivionBlockingPosture(world, victim) && !oblivionParalyzed(world, victim))
        {
            const auto& from = attacker.getRefData().getPosition();
            const auto& to = victim.getRefData().getPosition();
            const float dx = static_cast<float>(double(from.pos[0]) - to.pos[0]);
            const float dy = static_cast<float>(double(from.pos[1]) - to.pos[1]);
            const auto cone = ESM4::combatHitCone(to.rot[2], std::atan2(dx, dy),
                ESM4::buildCombatHitConeAngle(settings));
            if (cone.mInside)
            {
                // Native process +F8/+EC selects actual shield/weapon entries.
                // This branch supports an unarmed blocker: no blocking-item
                // wear exists. Equipped blocking items require the wear branch.
                auto& inventory = victim.getClass().getInventoryStore(victim);
                const auto shield = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedLeft);
                const auto weapon = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
                if ((shield != inventory.end() && shield->getType() == ESM::REC_ARMO)
                    || (weapon != inventory.end() && weapon->getType() == ESM::REC_WEAP))
                    return std::nullopt;
                const auto key = victim == player ? ESM::FormKey::dynamic("player", 1)
                    : victim.getCellRef().getFormKey();
                const auto base = [&](std::uint8_t av) { return victim == player ? service->getPlayerBaseValue(av)
                    : service->getNonPlayerBaseValue(key, av, world.getStore()); };
                const float fatigue = victim == player ? service->getPlayerValue(10)
                    : service->getNonPlayerValue(victim, 10);
                blockFraction = ESM4::blockFraction({integer(victim, 15), integer(victim, 7),
                    ESM4::combatFatigueRatio(fatigue, base(10)), ESM4::BlockEquipment::Unarmed},
                    ESM4::buildBlockSettings(settings), ESM4::buildPhysicalCombatSettings(settings));
                blockDebit = ESM4::blockContactCosts(base(15), integer(victim, 15), remaining,
                    blockFraction, false, ESM4::buildBlockCostSettings(settings),
                    ESM4::buildCombatMasterySettings(settings)).mFatigueDebit;
                remaining = static_cast<float>(double(remaining) * (1.0 - blockFraction));
            }
        }
        const auto role = victim == player ? ESM4::PlayerDamageRole::Victim
            : attacker == player ? ESM4::PlayerDamageRole::Attacker : ESM4::PlayerDamageRole::Unaffected;
        const auto damage = ESM4::physicalContactDamage({incoming.mHealth, incoming.mFatigue}, remaining,
            normalizedDifficulty, ESM4::buildDifficultyDamageMultiplier(settings), role);
        return OblivionUnarmedContactDamage{damage.mHealth, damage.mFatigue, blockDebit, blockFraction};
    }

    namespace
    {
        struct PreparedArmorWear
        {
            std::vector<OblivionPhysicalConditionChange> mConditions;
            std::optional<OblivionCombatRandomTransition> mRandom;
        };

        std::optional<PreparedArmorWear> prepareOrdinaryArmorWear(MWBase::World& world,
            OblivionCombatService& service, const MWWorld::Ptr& victim, float incoming,
            float absorbedFraction, std::span<const ESM4::GameSetting* const> settings)
        {
            PreparedArmorWear result;
            if (absorbedFraction <= 0)
                return result;
            const float wear = ESM4::armorWear(incoming, absorbedFraction, ESM4::buildDurabilitySettings(settings));
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
            if (player && world.getGodModeState())
                return result; // Original65FF10 suppresses condition after selection has consumed its draws.
            const auto& candidate = candidates[static_cast<unsigned>(*selected.mSlot)];
            const auto& ref = candidate.mItem.getCellRef();
            const double current = ref.getNativeItemCondition() ? double(*ref.getNativeItemCondition())
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
            if (*condition == 0)
                return std::nullopt; // Break unequip/drop/reactions require their own atomic policy.
            result.mConditions.push_back(captureOblivionPhysicalConditionChange(victim, candidate.mItem, *condition));
            return result;
        }
    }

    std::optional<OblivionWeaponContactDamage> resolveOblivionOrdinaryWeaponContact(
        MWBase::World& world, const MWWorld::Ptr& attacker, const MWWorld::Ptr& victim,
        const MWWorld::Ptr& item, float normalizedDifficulty, bool sneaking)
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
        if (!service || !eligible(attacker) || !eligible(victim) || attacker == victim || sneaking)
            return std::nullopt;
        // Validate authority before class queries can initialize shared caches.
        if (victim == player) (void)service->getPlayerValue(8);
        else (void)service->getNonPlayerValue(victim, 8);
        const auto resistance = victim == player ? service->getPlayerIntegerValue(65)
            : service->getNonPlayerIntegerValue(victim, 65);
        if (resistance != 0)
            return std::nullopt;
        const float incoming = oblivionOrdinaryWeaponContactDamage(world, attacker, item);
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
        if (!(attacker == player && world.getGodModeState()))
        {
            const auto& ref = item.getCellRef();
            const double current = ref.getNativeItemCondition() ? double(*ref.getNativeItemCondition())
                : ref.getCharge() < 0 ? double(weapon->mData.health)
                : double(ref.getCharge()) + ref.getChargeIntRemainder();
            condition = ESM4::nativeConditionAfterWear(current, ESM4::weaponWear(weapon->mData.damage, durability));
            if (condition && *condition == 0)
                return std::nullopt; // Native broken-item unequip/reactions remain a separate policy.
        }
        const auto armor = ESM4::mitigateArmor(incoming, oblivionArmorRating(world, victim),
            ESM4::buildArmorRatingSettings(settings).mSkillMaximum, false);
        if (oblivionBlockingPosture(world, victim) && !oblivionParalyzed(world, victim))
        {
            const auto& from = attacker.getRefData().getPosition();
            const auto& to = victim.getRefData().getPosition();
            const float dx = static_cast<float>(double(from.pos[0]) - to.pos[0]);
            const float dy = static_cast<float>(double(from.pos[1]) - to.pos[1]);
            if (ESM4::combatHitCone(to.rot[2], std::atan2(dx, dy), ESM4::buildCombatHitConeAngle(settings)).mInside)
                return std::nullopt; // Weapon hits also require block reactions when absorption is zero.
        }
        const auto role = victim == player ? ESM4::PlayerDamageRole::Victim
            : attacker == player ? ESM4::PlayerDamageRole::Attacker : ESM4::PlayerDamageRole::Unaffected;
        const auto damage = ESM4::physicalContactDamage({incoming, 0}, armor.mHealthDamage,
            normalizedDifficulty, ESM4::buildDifficultyDamageMultiplier(settings), role);
        auto wear = prepareOrdinaryArmorWear(world, *service, victim, incoming, armor.mAbsorbedFraction, settings);
        if (!wear)
            return std::nullopt;
        return OblivionWeaponContactDamage{damage.mHealth, condition, std::move(wear->mConditions), wear->mRandom};
    }

    float oblivionOrdinaryWeaponContactDamage(MWBase::World& world,
        const MWWorld::Ptr& attacker, const MWWorld::Ptr& item)
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
            integer(0), weapon->mData.damage, ratio, ESM4::combatFatigueRatio(current(10), baseFatigue)},
            ESM4::buildPhysicalCombatSettings(settings));
        const auto bonus = attacker == player ? service->getPlayerIntegerValue(42)
            : service->getNonPlayerIntegerValue(attacker, 42);
        const float total = static_cast<float>(double(damage) + bonus);
        if (!std::isfinite(total))
            throw std::invalid_argument("nonfinite native weapon damage");
        return total;
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
