#include "oblivionranged.hpp"

#include "oblivioncombat.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/oblivionprofileservices.hpp"
#include "../mwworld/worldimp.hpp"
#include <components/esm4/loadammo.hpp>
#include <components/esm4/loadweap.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace MWMechanics
{
    MWWorld::OblivionArrowLaunch sampleOblivionEquippedArrowLaunch(
        MWBase::World& world, const MWWorld::Ptr& actor, float playerBowTimer)
    {
        auto* native = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* service = native ? native->getOblivionCombatService() : nullptr;
        const bool player = actor == world.getPlayerPtr();
        if (!service || actor.isEmpty() || (!player && actor.getType() != ESM::REC_NPC_4))
            throw std::invalid_argument("native bow query requires Player/NPC authority");
        // Validate native binding before lazy shared inventory construction.
        if (player)
            (void)service->getPlayerValue(8);
        else
            (void)service->getNonPlayerValue(actor, 8);
        auto& inventory = actor.getClass().getInventoryStore(actor);
        const auto bow = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        const auto arrow = inventory.getSlot(MWWorld::InventoryStore::Slot_Ammunition);
        if (bow == inventory.end() || arrow == inventory.end()
            || bow->getContainerStore() != &inventory || arrow->getContainerStore() != &inventory
            || bow->getCellRef().getCount() <= 0 || arrow->getCellRef().getCount() <= 0)
            throw std::invalid_argument("native bow query requires equipped bow and ammunition instances");
        const auto& store = world.getStore();
        const auto bowId = MWWorld::OblivionProfileServices::nativeItemId(store, bow->getCellRef().getRefId());
        const auto arrowId = MWWorld::OblivionProfileServices::nativeItemId(store, arrow->getCellRef().getRefId());
        const auto* definition = store.get<ESM4::Weapon>().search(bowId);
        const auto bowKey = store.get<ESM4::Weapon>().findFormKey(bowId);
        const auto arrowKey = store.get<ESM4::Ammunition>().findFormKey(arrowId);
        if (!definition || definition->mData.type != 5 || !definition->mData.health || !bowKey || !arrowKey)
            throw std::invalid_argument("native bow query lacks winning stable bow/ammunition definitions");

        const auto& ref = bow->getCellRef();
        const double condition = ref.getNativeItemCondition() ? double(*ref.getNativeItemCondition())
            : ref.getCharge() < 0 ? double(definition->mData.health)
            : double(ref.getCharge()) + ref.getChargeIntRemainder();
        if (!std::isfinite(condition) || condition < 0)
            throw std::invalid_argument("invalid native bow condition");
        const auto current = [&](std::uint8_t av) {
            return player ? service->getPlayerValue(av) : service->getNonPlayerValue(actor, av);
        };
        const auto truncated = [&](std::uint8_t av) {
            const double value = std::trunc(double(current(av)));
            if (!std::isfinite(value) || value < std::numeric_limits<std::int32_t>::min()
                || value > std::numeric_limits<std::int32_t>::max())
                throw std::invalid_argument("native bow float AV conversion exceeds int32");
            return static_cast<std::int32_t>(value);
        };
        const auto actorKey = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
        const auto baseFatigue = player ? service->getPlayerBaseValue(10)
            : service->getNonPlayerBaseValue(actorKey, 10, store);
        const auto integer = [&](std::uint8_t av) {
            return player ? service->getPlayerIntegerValue(av) : service->getNonPlayerIntegerValue(actor, av);
        };
        return MWWorld::resolveOblivionArrowLaunch(store, world.getGameProfile(),
            {*bowKey, *arrowKey, player, playerBowTimer, truncated(28), truncated(7), truncated(3),
                static_cast<float>(condition / definition->mData.health),
                ESM4::combatFatigueRatio(current(10), baseFatigue), integer(28), integer(42)});
    }
}
