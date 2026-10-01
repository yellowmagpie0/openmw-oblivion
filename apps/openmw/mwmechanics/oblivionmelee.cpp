#include "oblivionmelee.hpp"

#include "oblivioncombat.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwphysics/actor.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/oblivionprofileservices.hpp"
#include <components/esm4/loadarmo.hpp>
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
            const float amount = ESM4::armorRating({armor.mData.armor, integer(isHeavy ? 18 : 27), luck, ratio},
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
