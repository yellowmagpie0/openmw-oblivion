#include "oblivionmelee.hpp"

#include "oblivioncombat.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwphysics/actor.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwworld/class.hpp"
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

    std::optional<MWWorld::Ptr> acquireOblivionMeleeContact(MWBase::World& world,
        std::uint64_t actionId, const MWWorld::Ptr& attacker,
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
        if (!service->isActionPending(actionId, source))
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
                candidate.mEligible = targetLife && targetLife->mPhase != ESM4::ActorLifePhase::Dead;
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
    bool commitOblivionOrdinaryMeleeMiss(MWBase::World& world, std::uint64_t actionId,
        const MWWorld::Ptr& attacker, const MWWorld::Ptr& selectedTarget,
        float reach, float weaponWeight)
    {
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        if (!service || attacker.isEmpty() || attacker.getType() == ESM::REC_CREA4)
            return false; // Creature process/fatigue eligibility is a separate caller.
        const auto actor = attacker == world.getPlayerPtr() ? ESM::FormKey::dynamic("player", 1)
            : attacker.getCellRef().getFormKey();
        if (!service->isOrdinaryMeleeContactPending(actionId, actor))
            return false;
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& record : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(record.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(record.mId));
        const float cost = ESM4::attackFatigueCost(weaponWeight, false,
            ESM4::buildAttackFatigueSettings(settings));
        const auto contact = acquireOblivionMeleeContact(world, actionId, attacker, selectedTarget, reach);
        if (!contact || !contact->isEmpty())
            return false;
        return world.commitOblivionPhysicalContact(actionId, attacker, {}, {-cost, 0, 0});
    }

}
