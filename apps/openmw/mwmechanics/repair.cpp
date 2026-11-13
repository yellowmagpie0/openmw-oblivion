#include "repair.hpp"

#include <components/misc/rng.hpp>
#include <components/misc/strings/format.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/debug/debuglog.hpp>
#include <components/esm4/loadgmst.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/repair.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/oblivionprofileservices.hpp"
#include "../mwworld/oblivionskilltraining.hpp"
#include "../mwworld/worldimp.hpp"
#include "../mwworld/worldmodel.hpp"

#include "actorutil.hpp"
#include "creaturestats.hpp"
#include "npcstats.hpp"
#include "oblivioncombat.hpp"

#include <array>

namespace
{
    ESM4::RepairSettings nativeRepairSettings(const MWWorld::ESMStore& store)
    {
        ESM4::RepairSettings settings;
        const std::array names{"fRepairArmorerBase", "fRepairArmorerMult", "fRepairStrengthMult",
            "fRepairSkillBreakBase", "fRepairSkillBreakMult", "fRepairBreakApprenticeMult", "fActorLuckSkillMult"};
        const std::array targets{&settings.mArmorerBase, &settings.mArmorerMultiplier,
            &settings.mStrengthMultiplier, &settings.mBreakBase, &settings.mBreakSkillMultiplier,
            &settings.mApprenticeBreakMultiplier, &settings.mLuckSkillMultiplier};
        for (const auto& entry : store.get<ESM4::GameSetting>())
        {
            const auto* winning = store.get<ESM4::GameSetting>().search(entry.mId);
            if (!winning || (winning->mFlags & ESM4::Rec_Deleted)) continue;
            for (std::size_t i = 0; i < names.size(); ++i)
                if (Misc::StringUtils::ciEqual(winning->mEditorId, names[i]))
                {
                    const auto* value = std::get_if<float>(&winning->mData);
                    if (!value) throw std::invalid_argument("native repair setting must be float");
                    *targets[i] = *value;
                }
            for (const auto& [name, target] : std::array{
                     std::pair{"iSkillApprenticeMin", &settings.mApprenticeMinimum},
                     std::pair{"iSkillMasterMin", &settings.mMasterMinimum}})
                if (Misc::StringUtils::ciEqual(winning->mEditorId, name))
                {
                    const auto* value = std::get_if<std::int32_t>(&winning->mData);
                    if (!value) throw std::invalid_argument("native repair mastery setting must be integer");
                    *target = *value;
                }
        }
        settings.validate();
        return settings;
    }

    bool nativeRepair(MWBase::World& world, const MWWorld::Ptr& player,
        const MWWorld::Ptr& tool, const MWWorld::Ptr& item)
    {
        const auto reject = [](const char* reason) {
            Log(Debug::Info) << "M13 repair rejected: reason=" << reason;
            return false;
        };
        Log(Debug::Info) << "M13 repair requested";
        auto* native = dynamic_cast<MWWorld::World*>(&world);
        auto* combat = native ? native->getOblivionCombatService() : nullptr;
        const auto* life = combat ? combat->findActorLife(ESM::FormKey::dynamic("player", 1)) : nullptr;
        if (!life || life->mPhase != ESM4::ActorLifePhase::Alive || tool.isEmpty() || item.isEmpty())
            return reject("actor-or-pointer");
        auto& inventory = player.getClass().getInventoryStore(player);
        if (tool.getContainerStore() != &inventory || item.getContainerStore() != &inventory)
            return reject("inventory-owner");
        if (tool.getCellRef().getCount() < 1 || item.getCellRef().getCount() != 1
            || tool.getCellRef().getNativeBoundSource() || item.getCellRef().getNativeBoundSource()
            || !tool.getCellRef().getNativeScriptOwner().isNull()
            || !item.getCellRef().getNativeScriptOwner().isNull()
            || !item.getClass().getEnchantment(item).empty()) return reject("item-admission");
        const auto& store = world.getStore();
        const auto* hammerRecord = store.get<ESM4::MiscItem>().search(tool.getCellRef().getRefId());
        if (!hammerRecord || (hammerRecord->mFlags & ESM4::Rec_Deleted)
            || !hammerRecord->mScriptId.isZeroOrUnset()) return reject("hammer-record");
        const auto hammer = MWWorld::OblivionProfileServices::itemDefinition(store, tool.getCellRef().getRefId());
        const auto hammerKey = store.get<ESM4::MiscItem>().findFormKey(tool.getCellRef().getRefId());
        auto definition = MWWorld::OblivionProfileServices::itemDefinition(store, item.getCellRef().getRefId());
        if (!hammer || !hammerKey || *hammerKey != ESM::FormKey::content("Oblivion.esm", 0xc) || !definition
            || (definition->mType != ESM4::InventoryItemType::Weapon
                && definition->mType != ESM4::InventoryItemType::Armor)
            || definition->mMaxCondition < 1) return reject("winning-definition");
        const auto supported = [](const auto* record) {
            return record && !(record->mFlags & ESM4::Rec_Deleted)
                && record->mEnchantment.isZeroOrUnset();
        };
        if (definition->mType == ESM4::InventoryItemType::Armor
                ? !supported(store.get<ESM4::Armor>().search(item.getCellRef().getRefId()))
                : !supported(store.get<ESM4::Weapon>().search(item.getCellRef().getRefId())))
            return reject("enchanted-or-deleted-item");
        const auto itemKey = definition->mType == ESM4::InventoryItemType::Armor
            ? store.get<ESM4::Armor>().findFormKey(item.getCellRef().getRefId())
            : store.get<ESM4::Weapon>().findFormKey(item.getCellRef().getRefId());
        if (!itemKey) return reject("item-identity");
        const auto winning = [&](const ESM::FormKey& key, ESM4::RecordTypes type) {
            const auto* winner = store.getFormKeyIndex().winner(key);
            return !winner || (!winner->mDeleted && winner->mRecordType == type);
        };
        if (!winning(*hammerKey, ESM4::REC_MISC)
            || !winning(*itemKey, definition->mType == ESM4::InventoryItemType::Armor
                ? ESM4::REC_ARMO : ESM4::REC_WEAP)) return reject("deleted-or-retyped-winner");
        definition->mBase = *itemKey;
        const auto settings = nativeRepairSettings(store);
        const int skill = combat->getPlayerIntegerValue(12);
        const int amount = ESM4::repairAmount(skill, combat->getPlayerIntegerValue(0), settings);
        const float before = item.getCellRef().getNativeItemCondition().value_or(definition->mMaxCondition);
        const auto after = ESM4::repairedCondition(before, definition->mMaxCondition, amount);
        if (!after) return reject("condition-or-amount");
        const float chance = ESM4::repairHammerBreakChance(skill, combat->getPlayerIntegerValue(7), settings);
        MWWorld::oblivionPlayerSkillProgressRequirement(store, player, 12);
        auto debit = inventory.prepareItemRemoval(tool, 1);
        if (debit->getCount() != 1 || !debit->isValid()) return reject("hammer-debit");
        MWWorld::CellRef condition(item.getCellRef());
        condition.setNativeItemCondition(*after);
        MWBase::Environment::get().getWorldModel()->registerPtr(item);
        world.breakInvisibility(player);
        const int roll = chance > 0.f ? Misc::Rng::roll0to99(world.getPrng()) : -1;
        const bool broken = roll >= 0 && roll < chance;
        if (broken && !debit->commit()) throw std::logic_error("native repair hammer debit changed");
        if (!item.getCellRef().swapNativeItemCondition(condition))
            throw std::logic_error("native repaired condition cannot publish");
        native->useOblivionPlayerSkill(12, 0);
        Log(Debug::Info) << "M13 repair committed: item=" << definition->mBase.serialize()
            << " condition=" << before << "->" << *after << " amount=" << amount
            << " roll=" << roll << " break_chance=" << chance << " hammer_broken=" << broken
            << " experience=" << player.getClass().getNpcStats(player).getSkill(ESM::Skill::Armorer).getProgress();
        if (broken)
        {
            debit->notify();
            MWBase::Environment::get().getWindowManager()->messageBox("Repair Hammer broke");
        }
        return true;
    }
}

namespace MWMechanics
{

    void Repair::repair(const MWWorld::Ptr& itemToRepair)
    {
        MWWorld::Ptr player = getPlayer();
        MWBase::World* world = MWBase::Environment::get().getWorld();
        if (world->getGameProfile() == ESM::GameProfile::Oblivion)
        {
            if (!nativeRepair(*world, player, mTool, itemToRepair))
                MWBase::Environment::get().getWindowManager()->messageBox("This item cannot be repaired");
            return;
        }
        MWWorld::LiveCellRef<ESM::Repair>* ref = mTool.get<ESM::Repair>();

        MWBase::Environment::get().getWorld()->breakInvisibility(player);

        // unstack tool if required
        player.getClass().getContainerStore(player).unstack(mTool);

        // reduce number of uses left
        int uses = mTool.getClass().getItemHealth(mTool);
        uses -= std::min(uses, 1);
        mTool.getCellRef().setCharge(uses);

        MWMechanics::CreatureStats& stats = player.getClass().getCreatureStats(player);

        float fatigueTerm = stats.getFatigueTerm();
        float pcStrength = stats.getAttribute(ESM::Attribute::Strength).getModified();
        float pcLuck = stats.getAttribute(ESM::Attribute::Luck).getModified();
        float armorerSkill = player.getClass().getSkill(player, ESM::Skill::Armorer);

        float fRepairAmountMult = MWBase::Environment::get()
                                      .getESMStore()
                                      ->get<ESM::GameSetting>()
                                      .find("fRepairAmountMult")
                                      ->mValue.getFloat();

        float toolQuality = ref->mBase->mData.mQuality;

        float x = (0.1f * pcStrength + 0.1f * pcLuck + armorerSkill) * fatigueTerm;

        auto& prng = MWBase::Environment::get().getWorld()->getPrng();
        int roll = Misc::Rng::roll0to99(prng);
        if (roll <= x)
        {
            int y = static_cast<int>(fRepairAmountMult * toolQuality * roll);
            y = std::max(1, y);

            // repair by 'y' points
            int charge = itemToRepair.getClass().getItemHealth(itemToRepair);
            charge = std::min(charge + y, itemToRepair.getClass().getItemMaxHealth(itemToRepair));
            itemToRepair.getCellRef().setCharge(charge);

            // attempt to re-stack item, in case it was fully repaired
            MWWorld::ContainerStoreIterator stacked = player.getClass().getContainerStore(player).restack(itemToRepair);

            // set the OnPCRepair variable on the item's script
            const ESM::RefId& script = stacked->getClass().getScript(itemToRepair);
            if (!script.empty())
                stacked->getRefData().getLocals().setVarByInt(script, "onpcrepair", 1);

            // increase skill
            player.getClass().skillUsageSucceeded(player, ESM::Skill::Armorer, ESM::Skill::Armorer_Repair);

            MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Repair"));
            MWBase::Environment::get().getWindowManager()->messageBox("#{sRepairSuccess}");
        }
        else
        {
            MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Repair Fail"));
            MWBase::Environment::get().getWindowManager()->messageBox("#{sRepairFailed}");
        }

        // tool used up?
        if (mTool.getCellRef().getCharge() == 0)
        {
            MWWorld::ContainerStore& store = player.getClass().getContainerStore(player);

            store.remove(mTool, 1);

            std::string message = MWBase::Environment::get()
                                      .getESMStore()
                                      ->get<ESM::GameSetting>()
                                      .find("sNotifyMessage51")
                                      ->mValue.getString();
            message = Misc::StringUtils::format(message, mTool.getClass().getName(mTool));

            MWBase::Environment::get().getWindowManager()->messageBox(message);

            // try to find a new tool of the same ID
            for (MWWorld::ContainerStoreIterator iter(store.begin()); iter != store.end(); ++iter)
            {
                if (iter->getCellRef().getRefId() == mTool.getCellRef().getRefId())
                {
                    mTool = *iter;

                    MWBase::Environment::get().getWindowManager()->playSound(ESM::RefId::stringRefId("Item Repair Up"));

                    break;
                }
            }
        }
    }

}
