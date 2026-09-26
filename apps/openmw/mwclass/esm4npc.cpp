#include "esm4npc.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>

#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadclot.hpp>
#include <components/esm4/loadlvli.hpp>
#include <components/esm4/loadlvln.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadotft.hpp>
#include <components/esm4/loadrace.hpp>
#include <components/esm4/playermechanics.hpp>

#include <components/misc/resourcehelpers.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/movement.hpp"
#include "../mwworld/actionopen.hpp"
#include "../mwworld/actiontalk.hpp"
#include "../mwworld/failedaction.hpp"
#include "../mwworld/manualref.hpp"
#include "../mwworld/customdata.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/oblivionprofileservices.hpp"
#include "../mwworld/oblivionactorstats.hpp"

#include "esm4base.hpp"

namespace MWClass
{
    template <class LevelledRecord, class TargetRecord>
    static std::vector<const TargetRecord*> withBaseTemplates(
        const TargetRecord* rec, int level = MWClass::ESM4Impl::sDefaultLevel)
    {
        std::vector<const TargetRecord*> res{ rec };
        while (true)
        {
            const TargetRecord* newRec
                = MWClass::ESM4Impl::resolveLevelled<ESM4::LevelledNpc, ESM4::Npc>(rec->mBaseTemplate, level);
            if (!newRec || newRec == rec)
                return res;
            res.push_back(rec = newRec);
        }
    }

    static const ESM4::Npc* chooseTemplate(const std::vector<const ESM4::Npc*>& recs, uint16_t flag)
    {
        for (const auto* rec : recs)
        {
            if (rec->mIsTES4)
                return rec;
            else if (rec->mIsFONV)
            {
                // TODO: FO3 should use this branch as well. But it is not clear how to distinguish FO3 from
                // TES5. Currently FO3 uses wrong template flags that can lead to "ESM4 NPC traits not found"
                // exception the NPC will not be added to the scene. But in any way it shouldn't cause a crash.
                if (!(rec->mBaseConfig.fo3.templateFlags & flag))
                    return rec;
            }
            else if (rec->mIsFO4)
            {
                if (!(rec->mBaseConfig.fo4.templateFlags & flag))
                    return rec;
            }
            else if (!(rec->mBaseConfig.tes5.templateFlags & flag))
                return rec;
        }
        return nullptr;
    }

    namespace
    {
        std::array<std::uint8_t, 8> attributes(const ESM4::AttributeValues& value)
        {
            return { value.strength, value.intelligence, value.willpower, value.agility, value.speed,
                value.endurance, value.personality, value.luck };
        }

        void fillInventory(ESM4NpcCustomData& data, const std::vector<ESM4::InventoryItem>& items,
            const MWWorld::ESMStore& store)
        {
            for (const ESM4::InventoryItem& item : items)
            {
                if (item.count == 0)
                    continue;
                const ESM::RefId nativeId(ESM::FormId::fromUint32(item.item));
                const ESM::RefId sharedId = MWWorld::OblivionProfileServices::sharedItemId(store, nativeId);
                try
                {
                    static_cast<void>(ESM4::inventoryItemCount(item));
                    const int count = item.count;
                    MWWorld::ManualRef ref(store, sharedId, count);
                    data.mInventoryStore.add(ref.getPtr(), count, false);
                }
                catch (const std::exception& error)
                {
                    Log(Debug::Warning) << "Oblivion NPC inventory item " << nativeId.toDebugString()
                                        << " was not projected: " << error.what();
                }
            }
        }

        void cacheEquipment(ESM4NpcCustomData& data, const MWWorld::ESMStore& store)
        {
            data.mEquippedArmor.clear();
            data.mEquippedClothing.clear();
            std::set<ESM::RefId> seen;
            const MWWorld::InventoryStore& inventory = data.mInventoryStore;
            for (int slot = 0; slot < MWWorld::InventoryStore::Slots; ++slot)
            {
                const MWWorld::ConstContainerStoreIterator item = inventory.getSlot(slot);
                if (item == inventory.end())
                    continue;
                const ESM::RefId id = item->getCellRef().getRefId();
                if (!seen.insert(id).second)
                    continue;
                if (const auto* armor = store.get<ESM4::Armor>().search(id))
                    data.mEquippedArmor.push_back(armor);
                else if (const auto* clothing = store.get<ESM4::Clothing>().search(id))
                    data.mEquippedClothing.push_back(clothing);
            }
        }

        const ESM4::Npc* baseData(const ESM4NpcCustomData& data)
        {
            return data.mBaseData != nullptr ? data.mBaseData : data.mTraits;
        }

        std::uint32_t actorFlags(const ESM4NpcCustomData& data)
        {
            const ESM4::Npc* record = baseData(data);
            return record == nullptr ? 0 : record->mBaseConfig.tes4.flags;
        }
    }

    ESM4NpcCustomData& ESM4Npc::getCustomData(const MWWorld::ConstPtr& ptr)
    {
        // Note: the argument is ConstPtr because this function is used in `getModel` and `getName`
        // which are virtual and work with ConstPtr. `getModel` and `getName` use custom data
        // because they require a lot of work including levelled records resolving and it would be
        // stupid to not to cache the results. Maybe we should stop using ConstPtr at all
        // to avoid such workarounds.
        MWWorld::RefData& refData = const_cast<MWWorld::RefData&>(ptr.getRefData());

        if (auto* data = refData.getCustomData())
            return data->asESM4NpcCustomData();

        auto data = std::make_unique<ESM4NpcCustomData>();

        // ConstPtr is used here because model/name queries are const virtuals,
        // while InventoryStore needs the mutable identity of its owner. Ptr's
        // underlying reference is still the same live object; this mirrors the
        // existing RefData custom-data cache semantics.
        const MWWorld::Ptr mutablePtr(const_cast<MWWorld::LiveCellRefBase*>(ptr.mRef),
            const_cast<MWWorld::CellStore*>(ptr.mCell));

        const MWWorld::ESMStore* store = MWBase::Environment::get().getESMStore();
        const ESM4::Npc* const base = ptr.get<ESM4::Npc>()->mBase;
        auto npcRecs = withBaseTemplates<ESM4::LevelledNpc, ESM4::Npc>(base);

        data->mTraits = chooseTemplate(npcRecs, ESM4::Npc::Template_UseTraits);

        if (data->mTraits == nullptr)
            Log(Debug::Warning) << "Traits are not found for ESM4 NPC base record: \"" << base->mEditorId << "\" ("
                                << ESM::RefId(base->mId) << ")";

        data->mBaseData = chooseTemplate(npcRecs, ESM4::Npc::Template_UseBaseData);

        if (data->mBaseData == nullptr)
            Log(Debug::Warning) << "Base data is not found for ESM4 NPC base record: \"" << base->mEditorId << "\" ("
                                << ESM::RefId(base->mId) << ")";

        if (data->mTraits != nullptr)
        {
            data->mRace = store->get<ESM4::Race>().find(data->mTraits->mRace);
            if (data->mTraits->mIsTES4)
                data->mIsFemale = data->mTraits->mBaseConfig.tes4.flags & ESM4::Npc::TES4_Female;
            else if (data->mTraits->mIsFONV)
                data->mIsFemale = data->mTraits->mBaseConfig.fo3.flags & ESM4::Npc::FO3_Female;
            else if (data->mTraits->mIsFO4)
                data->mIsFemale
                    = data->mTraits->mBaseConfig.fo4.flags & ESM4::Npc::TES5_Female; // FO4 flags are the same as TES5
            else
                data->mIsFemale = data->mTraits->mBaseConfig.tes5.flags & ESM4::Npc::TES5_Female;
        }

        const ESM4::Npc* statsRecord = data->mBaseData != nullptr ? data->mBaseData : data->mTraits;
        if (statsRecord == nullptr)
            statsRecord = base;

        if (statsRecord != nullptr)
        {
            if (statsRecord->mIsTES4)
            {
                const auto calculated = MWWorld::resolveOblivionActorConstructionStats(*store, statsRecord->mId,
                    statsRecord->mBaseConfig.tes4.flags & ESM4::Npc::TES4_PCLevelOffset);
                data->mNpcStats.initializeOblivionBaseStats(calculated.mAttributes,
                    {float(calculated.mHealth), float(calculated.mMagicka), float(calculated.mFatigue)},
                    calculated.mLevel);
                const auto& ids = MWWorld::oblivionSkillIds();
                for (std::size_t i = 0; i < ids.size(); ++i)
                    data->mNpcStats.getSkill(ids[i]).setBase(calculated.mSkills[i]);
            }
            else
            {
                const auto attrs = attributes(statsRecord->mData.attribs);
                for (std::size_t i = 0; i < attrs.size(); ++i)
                    data->mNpcStats.setAttribute(ESM::Attribute::indexToRefId(static_cast<int>(i)), attrs[i]);
                const std::array<std::uint8_t, 21> skills = { statsRecord->mData.skills.armorer,
                    statsRecord->mData.skills.athletics, statsRecord->mData.skills.blade, statsRecord->mData.skills.block,
                    statsRecord->mData.skills.blunt, statsRecord->mData.skills.handToHand,
                    statsRecord->mData.skills.heavyArmor, statsRecord->mData.skills.alchemy,
                    statsRecord->mData.skills.alteration, statsRecord->mData.skills.conjuration,
                    statsRecord->mData.skills.destruction, statsRecord->mData.skills.illusion,
                    statsRecord->mData.skills.mysticism, statsRecord->mData.skills.restoration,
                    statsRecord->mData.skills.acrobatics, statsRecord->mData.skills.lightArmor,
                    statsRecord->mData.skills.marksman, statsRecord->mData.skills.mercantile,
                    statsRecord->mData.skills.security, statsRecord->mData.skills.sneak,
                    statsRecord->mData.skills.speechcraft };
                const auto& ids = MWWorld::oblivionSkillIds();
                for (std::size_t i = 0; i < skills.size(); ++i)
                    data->mNpcStats.getSkill(ids[i]).setBase(skills[i]);

                data->mNpcStats.setHealth(MWMechanics::DynamicStat<float>(static_cast<float>(statsRecord->mData.health)));
                data->mNpcStats.setMagicka(
                    MWMechanics::DynamicStat<float>(static_cast<float>(statsRecord->mBaseConfig.tes4.baseSpell)));
                data->mNpcStats.setFatigue(
                    MWMechanics::DynamicStat<float>(static_cast<float>(statsRecord->mBaseConfig.tes4.fatigue)));
                data->mNpcStats.setLevel(std::max(1, static_cast<int>(statsRecord->mBaseConfig.tes4.levelOrOffset)));
            }
            data->mNpcStats.setAiSetting(MWMechanics::AiSetting::Hello, statsRecord->mAIData.energyLevel);
            data->mNpcStats.setAiSetting(MWMechanics::AiSetting::Fight, statsRecord->mAIData.aggression);
            data->mNpcStats.setAiSetting(MWMechanics::AiSetting::Flee, statsRecord->mAIData.confidence);
            data->mNpcStats.setAiSetting(MWMechanics::AiSetting::Alarm, statsRecord->mAIData.responsibility);

            data->mInventoryStore.setPtr(mutablePtr);
            fillInventory(*data, statsRecord->mInventory, *store);
            // Inventory projection may advance the WorldModel pointer
            // registry. Refresh the owner SafePtr before native equipment
            // initialization accesses the owning actor through InventoryStore.
            data->mInventoryStore.setPtr(mutablePtr);
        }

        refData.setCustomData(std::move(data));
        ESM4NpcCustomData& res = refData.getCustomData()->asESM4NpcCustomData();
        res.mInventoryStore.setPtr(mutablePtr);
        MWWorld::OblivionProfileServices::equipNativeApparel(res.mInventoryStore, *store);
        res.mInventoryStore.setPtr(mutablePtr);
        cacheEquipment(res, *store);
        return res;
    }

    MWMechanics::CreatureStats& ESM4Npc::getCreatureStats(const MWWorld::Ptr& ptr) const
    {
        return getCustomData(ptr).mNpcStats;
    }

    MWMechanics::NpcStats& ESM4Npc::getNpcStats(const MWWorld::Ptr& ptr) const
    {
        return getCustomData(ptr).mNpcStats;
    }

    MWWorld::ContainerStore& ESM4Npc::getContainerStore(const MWWorld::Ptr& ptr) const
    {
        return getCustomData(ptr).mInventoryStore;
    }

    MWWorld::InventoryStore& ESM4Npc::getInventoryStore(const MWWorld::Ptr& ptr) const
    {
        return getCustomData(ptr).mInventoryStore;
    }

    ESM::RefId ESM4Npc::getScript(const MWWorld::ConstPtr& ptr) const
    {
        return ESM::RefId(ptr.get<ESM4::Npc>()->mBase->mScriptId);
    }

    float ESM4Npc::getCapacity(const MWWorld::Ptr& ptr) const
    {
        return getNpcStats(ptr).getAttribute(ESM::Attribute::Strength).getModified() * 5.f;
    }

    float ESM4Npc::getArmorRating(const MWWorld::Ptr& ptr, bool) const
    {
        float result = 0.f;
        MWWorld::InventoryStore& inventory = getInventoryStore(ptr);
        for (int slot = 0; slot < MWWorld::InventoryStore::Slots; ++slot)
        {
            const MWWorld::ContainerStoreIterator item = inventory.getSlot(slot);
            if (item != inventory.end())
                result += item->getClass().getArmorRating(*item);
        }
        return result;
    }

    bool ESM4Npc::isEssential(const MWWorld::ConstPtr& ptr) const
    {
        const auto& data = getCustomData(ptr);
        return (actorFlags(data) & ESM4::Npc::TES4_Essential) != 0;
    }

    int ESM4Npc::getServices(const MWWorld::ConstPtr&) const
    {
        return 0;
    }

    bool ESM4Npc::isPersistent(const MWWorld::ConstPtr& ptr) const
    {
        return (ptr.get<ESM4::Npc>()->mBase->mFlags & ESM4::Rec_Persistent) != 0;
    }

    MWMechanics::Movement& ESM4Npc::getMovementSettings(const MWWorld::Ptr& ptr) const
    {
        return getCustomData(ptr).mMovement;
    }

    float ESM4Npc::getMaxSpeed(const MWWorld::Ptr& ptr) const
    {
        const MWMechanics::NpcStats& stats = getNpcStats(ptr);
        if (stats.isParalyzed() || stats.getKnockedDown() || stats.isDead())
            return 0.f;
        const MWBase::World* world = MWBase::Environment::get().getWorld();
        const bool running = MWBase::Environment::get().getMechanicsManager()->isRunning(ptr);
        const bool sneaking = MWBase::Environment::get().getMechanicsManager()->isSneaking(ptr);
        if (world->isSwimming(ptr))
            return getSwimSpeed(ptr);
        return running && !sneaking ? getRunSpeed(ptr) : getWalkSpeed(ptr);
    }

    float ESM4Npc::getJump(const MWWorld::Ptr& ptr) const
    {
        const MWMechanics::NpcStats& stats = getNpcStats(ptr);
        if (stats.isParalyzed() || stats.getKnockedDown() || stats.isDead())
            return 0.f;
        return ESM4::playerJumpVelocity(getSkill(ptr, ESM::Skill::Acrobatics), getEncumbrance(ptr), getCapacity(ptr))
            * std::sqrt(std::max(0.f, stats.getFatigueTerm()));
    }

    float ESM4Npc::getWalkSpeed(const MWWorld::Ptr& ptr) const
    {
        const MWMechanics::NpcStats& stats = getNpcStats(ptr);
        return ESM4::playerWalkSpeed(stats.getAttribute(ESM::Attribute::Speed).getModified(), getEncumbrance(ptr),
            getCapacity(ptr), MWBase::Environment::get().getMechanicsManager()->isSneaking(ptr));
    }

    float ESM4Npc::getRunSpeed(const MWWorld::Ptr& ptr) const
    {
        const MWMechanics::NpcStats& stats = getNpcStats(ptr);
        return ESM4::playerRunSpeed(stats.getAttribute(ESM::Attribute::Speed).getModified(), getEncumbrance(ptr),
            getCapacity(ptr));
    }

    float ESM4Npc::getSwimSpeed(const MWWorld::Ptr& ptr) const
    {
        return MWBase::Environment::get().getMechanicsManager()->isRunning(ptr) ? getRunSpeed(ptr) : getWalkSpeed(ptr);
    }

    float ESM4Npc::getSkill(const MWWorld::Ptr& ptr, ESM::RefId id) const
    {
        return getNpcStats(ptr).getSkill(id).getModified();
    }

    int ESM4Npc::getBaseFightRating(const MWWorld::ConstPtr& ptr) const
    {
        return ptr.get<ESM4::Npc>()->mBase->mAIData.aggression;
    }

    ESM::RefId ESM4Npc::getPrimaryFaction(const MWWorld::ConstPtr& ptr) const
    {
        return ESM::RefId(ESM::FormId::fromUint32(ptr.get<ESM4::Npc>()->mBase->mFaction.faction));
    }

    int ESM4Npc::getPrimaryFactionRank(const MWWorld::ConstPtr& ptr) const
    {
        if (ptr.get<ESM4::Npc>()->mBase->mFaction.faction == 0)
            return -1;
        return ptr.get<ESM4::Npc>()->mBase->mFaction.rank;
    }

    std::unique_ptr<MWWorld::Action> ESM4Npc::activate(
        const MWWorld::Ptr& ptr, const MWWorld::Ptr&) const
    {
        const MWMechanics::CreatureStats& stats = getCreatureStats(ptr);
        if (stats.isDead())
            return std::make_unique<MWWorld::ActionOpen>(ptr);
        if (!stats.getKnockedDown())
            return std::make_unique<MWWorld::ActionTalk>(ptr);
        return std::make_unique<MWWorld::FailedAction>();
    }

    void ESM4Npc::getModelsToPreload(
        const MWWorld::ConstPtr& ptr, std::vector<VFS::Path::NormalizedView>& models) const
    {
        const auto model = getModel(ptr);
        if (!model.empty())
            models.push_back(model);
        const auto& data = getCustomData(ptr);
        for (const ESM4::Armor* armor : data.mEquippedArmor)
            if (!armor->mModel.empty())
                models.push_back(armor->mModel.getNormalized());
        for (const ESM4::Clothing* clothing : data.mEquippedClothing)
            if (!clothing->mModel.empty())
                models.push_back(clothing->mModel.getNormalized());
    }

    const std::vector<const ESM4::Armor*>& ESM4Npc::getEquippedArmor(const MWWorld::Ptr& ptr)
    {
        return getCustomData(ptr).mEquippedArmor;
    }

    const std::vector<const ESM4::Clothing*>& ESM4Npc::getEquippedClothing(const MWWorld::Ptr& ptr)
    {
        return getCustomData(ptr).mEquippedClothing;
    }

    const ESM4::Npc* ESM4Npc::getTraitsRecord(const MWWorld::Ptr& ptr)
    {
        return getCustomData(ptr).mTraits;
    }

    const ESM4::Race* ESM4Npc::getRace(const MWWorld::Ptr& ptr)
    {
        return getCustomData(ptr).mRace;
    }

    bool ESM4Npc::isFemale(const MWWorld::Ptr& ptr)
    {
        return getCustomData(ptr).mIsFemale;
    }

    VFS::Path::NormalizedView ESM4Npc::getModel(const MWWorld::ConstPtr& ptr) const
    {
        const ESM4NpcCustomData& data = getCustomData(ptr);
        if (data.mTraits == nullptr)
            return {};
        if (data.mTraits->mIsTES4)
            return data.mTraits->mModel.getNormalized();
        if (data.mRace == nullptr)
            return {};
        return data.mIsFemale ? data.mRace->mModelFemale.getNormalized() : data.mRace->mModelMale.getNormalized();
    }

    std::string_view ESM4Npc::getName(const MWWorld::ConstPtr& ptr) const
    {
        const ESM4::Npc* const baseData = getCustomData(ptr).mBaseData;
        if (baseData == nullptr)
            return {};
        return baseData->mFullName;
    }

    void ESM4Npc::adjustScale(const MWWorld::ConstPtr& ptr, osg::Vec3f& scale, bool rendering) const
    {
        if (!rendering)
            return;
        const ESM4NpcCustomData& data = getCustomData(ptr);
        if (data.mRace == nullptr)
            return;
        const float height = data.mIsFemale ? data.mRace->mHeightFemale : data.mRace->mHeightMale;
        const float weight = data.mIsFemale ? data.mRace->mWeightFemale : data.mRace->mWeightMale;
        scale.x() *= weight;
        scale.y() *= weight;
        scale.z() *= height;
    }
}
