#include "oblivionmagiccaster.hpp"

#include "oblivioncombat.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/oblivionprofileservices.hpp"
#include "../mwworld/worldimp.hpp"

#include <components/esm/records.hpp>
#include <components/esm4/combatsettings.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadgmst.hpp>

#include <array>
#include <set>
#include <stdexcept>
#include <vector>

namespace MWMechanics
{
    float oblivionCombatSpellPower(MWBase::World& world, const MWWorld::Ptr& actor)
    {
        auto* nativeWorld = world.getGameProfile() == ESM::GameProfile::Oblivion
            ? dynamic_cast<MWWorld::World*>(&world) : nullptr;
        auto* service = nativeWorld ? nativeWorld->getOblivionCombatService() : nullptr;
        if (!service || actor.isEmpty())
            throw std::invalid_argument("native combat spell power requires a native actor");
        const bool player = actor == world.getPlayerPtr();
        if (!player && actor.getType() != ESM::REC_NPC_4 && actor.getType() != ESM::REC_CREA4)
            throw std::invalid_argument("native combat spell power requires a native actor");
        const auto key = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
        const auto* values = service->findActorValues(key);
        if (!values)
            throw std::invalid_argument("native combat spell power requires actor-value authority");
        if (!player)
            (void)service->getNonPlayerValue(actor, 8); // Validate reference/base/owner identity before the constant branch.
        // Original NPC5E5630 always returns one, including Creature. Player's
        // god-mode branch delegates to it before reading skills/equipment.
        if (!player || world.getGodModeState())
            return 1.f;

        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& record : world.getStore().get<ESM4::GameSetting>())
            if (seen.insert(record.mId).second)
                settings.push_back(world.getStore().get<ESM4::GameSetting>().search(record.mId));
        const auto casting = ESM4::buildCombatSpellPowerSettings(settings);
        const auto armor = ESM4::buildArmorMasterySettings(settings);
        std::array<bool, 7> light{}, heavy{};
        auto& inventory = actor.getClass().getInventoryStore(actor);
        const auto carried = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedLeft);
        for (auto item = inventory.begin(); item != inventory.end(); ++item)
        {
            if (!inventory.isEquipped(*item) || item->getType() != ESM::REC_ARMO)
                continue; // Original486790(slot,1) excludes clothing.
            const auto id = MWWorld::OblivionProfileServices::nativeItemId(
                world.getStore(), item->getCellRef().getRefId());
            const auto* form = id.getIf<ESM::FormId>();
            const auto* definition = form ? world.getStore().get<ESM4::Armor>().search(*form) : nullptr;
            if (!definition)
                throw std::invalid_argument("casting armor has no winning native definition");
            for (unsigned i = 0; i < light.size(); ++i)
            {
                const unsigned slot = i == 6 ? 13 : i;
                if (!(definition->mArmorFlags & (1u << slot))) continue;
                // This is the actually worn shield entry, independent of
                // held-block intent. Low processes expose no such entry.
                if (slot == 13 && (values->mProcess != ESM4::ActorValueProcess::Active
                        || carried == inventory.end() || *carried != *item)) continue;
                if (light[i] || heavy[i])
                    throw std::invalid_argument("overlapping native casting armor slots");
                ((definition->mGeneralFlags & ESM4::Armor::TES4_HeavyArmor) ? heavy : light)[i] = true;
            }
        }
        return ESM4::playerCombatSpellPower({service->getPlayerIntegerValue(10), service->getPlayerBaseValue(10),
            service->getPlayerIntegerValue(27), service->getPlayerIntegerValue(18),
            ESM4::armorCoverage(light, armor), ESM4::armorCoverage(heavy, armor)}, casting);
    }
}
