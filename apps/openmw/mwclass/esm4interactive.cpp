#include "esm4interactive.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <components/debug/debuglog.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/doorstate.hpp>
#include <components/esm3/loadskil.hpp>
#include <components/esm4/playermechanics.hpp>

#include "../mwbase/environment.hpp"
#include "../mwmechanics/oblivioncombat.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/oblivionai.hpp"
#include "../mwworld/actionopen.hpp"
#include "../mwworld/actionhorse.hpp"
#include "../mwworld/actiontalk.hpp"
#include "../mwphysics/physicssystem.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"

#include "../mwworld/actiondoor.hpp"
#include "../mwworld/actionteleport.hpp"
#include "../mwworld/failedaction.hpp"
#include "../mwworld/manualref.hpp"
#include "../mwworld/oblivionprofileservices.hpp"
#include "../mwworld/oblivionactorstats.hpp"
#include "../mwworld/oblivioninteraction.hpp"
#include "../mwworld/worldimp.hpp"

namespace MWClass
{
    namespace
    {
        void fillCreatureInventory(ESM4CreatureCustomData& data, const std::vector<ESM4::InventoryItem>& items,
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
                    Log(Debug::Warning) << "Oblivion creature inventory item " << nativeId.toDebugString()
                                        << " was not projected: " << error.what();
                }
            }
        }

        bool isRunning(const MWWorld::Ptr& ptr)
        {
            return MWBase::Environment::get().getMechanicsManager()->isRunning(ptr);
        }

        bool isSneaking(const MWWorld::Ptr& ptr)
        {
            return MWBase::Environment::get().getMechanicsManager()->isSneaking(ptr);
        }

        void setActionSound(MWWorld::Action& action, const ESM::FormId& sound)
        {
            if (!sound.isZeroOrUnset())
            {
                action.setSound(ESM::RefId(sound));
                Log(Debug::Info) << "M5 sound selection: form=" << ESM::RefId(sound).toDebugString();
            }
        }

        void appendReferenceState(MWGui::ToolTipInfo& info, const MWWorld::ConstPtr& ptr)
        {
            if (ptr.getCellRef().isLocked())
                info.text += "\nLocked (level " + std::to_string(std::max(0, ptr.getCellRef().getLockLevel())) + ")";
            if (!ptr.getCellRef().getOwner().empty())
                info.text += "\nOwned";
        }
    }

    ESM4Activator::ESM4Activator()
        : ESM4InteractiveBase(ESM4::Activator::sRecordId)
    {
    }

    std::string_view ESM4Activator::getName(const MWWorld::ConstPtr& ptr) const
    {
        const ESM4::Activator* activator = ptr.get<ESM4::Activator>()->mBase;
        if (!activator->mFullName.empty())
            return activator->mFullName;
        if (!activator->mActivationPrompt.empty())
            return activator->mActivationPrompt;
        return {};
    }

    void ESM4Activator::insertObjectPhysics(const MWWorld::Ptr& ptr, const std::string& model,
        const osg::Quat& rotation, MWPhysics::PhysicsSystem& physics) const
    {
        // Scripted activators can transition collision nodes at runtime. Keep their
        // collision representation available even when the visual NIF marks the
        // corresponding geometry as camera-only or non-colliding.
        const bool scripted = !ptr.get<ESM4::Activator>()->mBase->mScriptId.isZeroOrUnset();
        physics.addObject(ptr, VFS::Path::toNormalized(model), rotation, MWPhysics::CollisionType_World,
            !scripted);
    }

    std::unique_ptr<MWWorld::Action> ESM4Activator::activate(
        const MWWorld::Ptr& ptr, const MWWorld::Ptr&) const
    {
        auto action = std::make_unique<MWWorld::OblivionInteractionAction>(
            ptr, MWWorld::OblivionInteractionKind::Activator);
        setActionSound(*action, ptr.get<ESM4::Activator>()->mBase->mActivationSound);
        return action;
    }

    ESM4Book::ESM4Book()
        : ESM4InteractiveBase(ESM4::Book::sRecordId)
    {
    }

    std::unique_ptr<MWWorld::Action> ESM4Book::activate(const MWWorld::Ptr& ptr, const MWWorld::Ptr&) const
    {
        return std::make_unique<MWWorld::OblivionInteractionAction>(ptr, MWWorld::OblivionInteractionKind::Book);
    }

    ESM4Container::ESM4Container()
        : ESM4InteractiveBase(ESM4::Container::sRecordId)
    {
    }

    std::unique_ptr<MWWorld::Action> ESM4Container::activate(
        const MWWorld::Ptr& ptr, const MWWorld::Ptr&) const
    {
        auto action = std::make_unique<MWWorld::OblivionInteractionAction>(
            ptr, MWWorld::OblivionInteractionKind::Container);
        if (!ptr.getCellRef().isLocked() && ptr.getCellRef().getOwner().empty())
            setActionSound(*action, ptr.get<ESM4::Container>()->mBase->mOpenSound);
        return action;
    }

    MWGui::ToolTipInfo ESM4Container::getToolTipInfo(const MWWorld::ConstPtr& ptr, int count) const
    {
        MWGui::ToolTipInfo info = ESM4InteractiveBase::getToolTipInfo(ptr, count);
        appendReferenceState(info, ptr);
        return info;
    }

    ESM4Creature::ESM4Creature()
        : MWWorld::RegisteredClass<ESM4Creature, Actor>(ESM4::Creature::sRecordId)
    {
    }

    void ESM4Creature::ensureCustomData(const MWWorld::Ptr& ptr) const
    {
        if (ptr.getRefData().getCustomData())
            return;

        auto data = std::make_unique<ESM4CreatureCustomData>();
        const ESM4::Creature* base = ptr.get<ESM4::Creature>()->mBase;
        const MWWorld::ESMStore* store = MWBase::Environment::get().getESMStore();
        if (base->mAttackReach)
        {
            const auto calculated = MWWorld::resolveOblivionActorConstructionStats(*store, base->mId,
                base->mBaseConfig.tes4.flags & ESM4::Creature::TES4_PCLevelOffset);
            data->mCreatureStats.initializeOblivionBaseStats(calculated.mAttributes,
                {float(calculated.mHealth), float(calculated.mMagicka), float(calculated.mFatigue)},
                calculated.mLevel);
            data->mNativeSkills.emplace();
            std::copy(calculated.mSkills.begin(), calculated.mSkills.end(), data->mNativeSkills->begin());
            data->mNativeDamage = calculated.mNaturalDamage;
        }
        else
        {
            const auto attributes = std::array<std::uint8_t, 8>{ base->mData.attribs.strength,
                base->mData.attribs.intelligence, base->mData.attribs.willpower, base->mData.attribs.agility,
                base->mData.attribs.speed, base->mData.attribs.endurance, base->mData.attribs.personality,
                base->mData.attribs.luck };
            for (std::size_t i = 0; i < attributes.size(); ++i)
                data->mCreatureStats.setAttribute(ESM::Attribute::indexToRefId(static_cast<int>(i)), attributes[i]);
            data->mCreatureStats.setHealth(MWMechanics::DynamicStat<float>(static_cast<float>(base->mData.health)));
            data->mCreatureStats.setMagicka(
                MWMechanics::DynamicStat<float>(static_cast<float>(base->mBaseConfig.tes4.baseSpell)));
            data->mCreatureStats.setFatigue(
                MWMechanics::DynamicStat<float>(static_cast<float>(base->mBaseConfig.tes4.fatigue)));
            data->mCreatureStats.setLevel(std::max(1, static_cast<int>(base->mBaseConfig.tes4.levelOrOffset)));
        }
        data->mCreatureStats.setAiSetting(MWMechanics::AiSetting::Hello, base->mAIData.energyLevel);
        data->mCreatureStats.setAiSetting(MWMechanics::AiSetting::Fight, base->mAIData.aggression);
        data->mCreatureStats.setAiSetting(MWMechanics::AiSetting::Flee, base->mAIData.confidence);
        data->mCreatureStats.setAiSetting(MWMechanics::AiSetting::Alarm, base->mAIData.responsibility);
        data->mInventoryStore.setPtr(ptr);
        fillCreatureInventory(*data, base->mInventory, *store);
        // Adding projected inventory stacks can advance the WorldModel pointer
        // registry. Refresh the owner SafePtr before native equipment
        // initialization accesses the owning actor.
        data->mInventoryStore.setPtr(ptr);
        ptr.getRefData().setCustomData(std::move(data));
        ESM4CreatureCustomData& initialized = ptr.getRefData().getCustomData()->asESM4CreatureCustomData();
        initialized.mInventoryStore.setPtr(ptr);
        MWWorld::OblivionProfileServices::equipNativeApparel(initialized.mInventoryStore, *store);
        initialized.mInventoryStore.setPtr(ptr);
        if (auto* world = dynamic_cast<MWWorld::World*>(MWBase::Environment::get().getWorldOrNull()))
            if (auto* combat = world->getOblivionCombatService())
                if (const auto* values = combat->findActorValues(ptr.getCellRef().getFormKey());
                    values && combat->findActorLife(values->mActor))
                    world->initializeOblivionNonPlayerActor(ptr, values->mProcess);
    }

    MWWorld::Ptr ESM4Creature::copyToCellImpl(const MWWorld::ConstPtr& ptr, MWWorld::CellStore& cell) const
    {
        const MWWorld::LiveCellRef<ESM4::Creature>* ref = ptr.get<ESM4::Creature>();
        MWWorld::Ptr result(cell.insert(ref), &cell);
        if (result.getRefData().getCustomData())
            result.getClass().getInventoryStore(result).setPtr(result);
        return result;
    }

    std::string_view ESM4Creature::getName(const MWWorld::ConstPtr& ptr) const
    {
        return ptr.get<ESM4::Creature>()->mBase->mFullName;
    }

    MWGui::ToolTipInfo ESM4Creature::getToolTipInfo(const MWWorld::ConstPtr& ptr, int count) const
    {
        return ESM4Impl::getToolTipInfo(getName(ptr), count);
    }

    MWMechanics::CreatureStats& ESM4Creature::getCreatureStats(const MWWorld::Ptr& ptr) const
    {
        ensureCustomData(ptr);
        return ptr.getRefData().getCustomData()->asESM4CreatureCustomData().mCreatureStats;
    }

    MWWorld::ContainerStore& ESM4Creature::getContainerStore(const MWWorld::Ptr& ptr) const
    {
        ensureCustomData(ptr);
        return ptr.getRefData().getCustomData()->asESM4CreatureCustomData().mInventoryStore;
    }

    MWWorld::InventoryStore& ESM4Creature::getInventoryStore(const MWWorld::Ptr& ptr) const
    {
        ensureCustomData(ptr);
        return ptr.getRefData().getCustomData()->asESM4CreatureCustomData().mInventoryStore;
    }

    ESM::RefId ESM4Creature::getScript(const MWWorld::ConstPtr& ptr) const
    {
        return ESM::RefId(ptr.get<ESM4::Creature>()->mBase->mScriptId);
    }

    float ESM4Creature::getCapacity(const MWWorld::Ptr& ptr) const
    {
        return getCreatureStats(ptr).getAttribute(ESM::Attribute::Strength).getModified() * 5.f;
    }

    float ESM4Creature::getArmorRating(const MWWorld::Ptr& ptr, bool) const
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

    bool ESM4Creature::isEssential(const MWWorld::ConstPtr& ptr) const
    {
        return (ptr.get<ESM4::Creature>()->mBase->mBaseConfig.tes4.flags & ESM4::Creature::TES4_Essential) != 0;
    }

    bool ESM4Creature::isPersistent(const MWWorld::ConstPtr& ptr) const
    {
        return (ptr.get<ESM4::Creature>()->mBase->mFlags & ESM4::Rec_Persistent) != 0;
    }

    int ESM4Creature::getServices(const MWWorld::ConstPtr&) const
    {
        return 0;
    }

    MWMechanics::Movement& ESM4Creature::getMovementSettings(const MWWorld::Ptr& ptr) const
    {
        ensureCustomData(ptr);
        return ptr.getRefData().getCustomData()->asESM4CreatureCustomData().mMovement;
    }

    float ESM4Creature::getMaxSpeed(const MWWorld::Ptr& ptr) const
    {
        const MWMechanics::CreatureStats& stats = getCreatureStats(ptr);
        if (stats.isParalyzed() || stats.getKnockedDown() || stats.isDead())
            return 0.f;
        if (MWBase::Environment::get().getWorld()->isSwimming(ptr))
            return getSwimSpeed(ptr);
        return isRunning(ptr) ? getRunSpeed(ptr) : getWalkSpeed(ptr);
    }

    float ESM4Creature::getJump(const MWWorld::Ptr& ptr) const
    {
        const MWMechanics::CreatureStats& stats = getCreatureStats(ptr);
        if (stats.isParalyzed() || stats.getKnockedDown() || stats.isDead())
            return 0.f;
        return ESM4::playerJumpVelocity(50.f, getEncumbrance(ptr), getCapacity(ptr));
    }

    float ESM4Creature::getWalkSpeed(const MWWorld::Ptr& ptr) const
    {
        return ESM4::playerWalkSpeed(getCreatureStats(ptr).getAttribute(ESM::Attribute::Speed).getModified(),
            getEncumbrance(ptr), getCapacity(ptr), isSneaking(ptr));
    }

    float ESM4Creature::getRunSpeed(const MWWorld::Ptr& ptr) const
    {
        return ESM4::playerRunSpeed(getCreatureStats(ptr).getAttribute(ESM::Attribute::Speed).getModified(),
            getEncumbrance(ptr), getCapacity(ptr));
    }

    float ESM4Creature::getSwimSpeed(const MWWorld::Ptr& ptr) const
    {
        return isRunning(ptr) ? getRunSpeed(ptr) : getWalkSpeed(ptr);
    }

    bool ESM4Creature::isBipedal(const MWWorld::ConstPtr& ptr) const
    {
        return (ptr.get<ESM4::Creature>()->mBase->mBaseConfig.tes4.flags & ESM4::Creature::TES4_Biped) != 0;
    }

    bool ESM4Creature::canFly(const MWWorld::ConstPtr& ptr) const
    {
        return (ptr.get<ESM4::Creature>()->mBase->mBaseConfig.tes4.flags & ESM4::Creature::TES4_Flies) != 0;
    }

    bool ESM4Creature::canSwim(const MWWorld::ConstPtr& ptr) const
    {
        return (ptr.get<ESM4::Creature>()->mBase->mBaseConfig.tes4.flags
            & (ESM4::Creature::TES4_Swims | ESM4::Creature::TES4_Biped)) != 0;
    }

    bool ESM4Creature::canWalk(const MWWorld::ConstPtr& ptr) const
    {
        return (ptr.get<ESM4::Creature>()->mBase->mBaseConfig.tes4.flags
            & (ESM4::Creature::TES4_Walks | ESM4::Creature::TES4_Biped)) != 0;
    }

    int ESM4Creature::getBaseFightRating(const MWWorld::ConstPtr& ptr) const
    {
        return ptr.get<ESM4::Creature>()->mBase->mAIData.aggression;
    }

    ESM::RefId ESM4Creature::getPrimaryFaction(const MWWorld::ConstPtr& ptr) const
    {
        return ESM::RefId(ESM::FormId::fromUint32(ptr.get<ESM4::Creature>()->mBase->mFaction.faction));
    }

    int ESM4Creature::getPrimaryFactionRank(const MWWorld::ConstPtr& ptr) const
    {
        return ptr.get<ESM4::Creature>()->mBase->mFaction.faction == 0
            ? -1
            : ptr.get<ESM4::Creature>()->mBase->mFaction.rank;
    }

    float ESM4Creature::getSkill(const MWWorld::Ptr& ptr, ESM::RefId id) const
    {
        ensureCustomData(ptr);
        const auto& data = ptr.getRefData().getCustomData()->asESM4CreatureCustomData();
        if (data.mNativeSkills)
        {
            const auto& ids = MWWorld::oblivionSkillIds();
            const auto found = std::find(ids.begin(), ids.end(), id);
            if (found == ids.end())
                throw std::invalid_argument("unsupported skill on a native creature");
            // The live Creature AV wrappers remap Marksman (AV28) to
            // combat (AV12), unlike the base-form getter's seven-skill groups.
            const auto index = id == ESM::Skill::Marksman ? 0 : std::distance(ids.begin(), found);
            return (*data.mNativeSkills)[index];
        }
        const ESM::Skill* skill = MWBase::Environment::get().getESMStore()->get<ESM::Skill>().search(id);
        if (skill == nullptr)
            return 0.f;
        switch (skill->mData.mSpecialization)
        {
            case ESM::Class::Combat:
                return ptr.get<ESM4::Creature>()->mBase->mData.combat;
            case ESM::Class::Magic:
                return ptr.get<ESM4::Creature>()->mBase->mData.magic;
            case ESM::Class::Stealth:
                return ptr.get<ESM4::Creature>()->mBase->mData.stealth;
            default:
                return 0.f;
        }
    }

    VFS::Path::NormalizedView ESM4Creature::getModel(const MWWorld::ConstPtr& ptr) const
    {
        return ptr.get<ESM4::Creature>()->mBase->mModel.getNormalized();
    }

    void ESM4Creature::adjustScale(const MWWorld::ConstPtr& ptr, osg::Vec3f& scale, bool rendering) const
    {
        if (rendering)
            scale *= ptr.get<ESM4::Creature>()->mBase->mBaseScale;
    }

    void ESM4Creature::insertObjectRendering(const MWWorld::Ptr& ptr, const std::string& model,
        MWRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
            renderingInterface.getObjects().insertCreature4(ptr, model);
    }

    std::unique_ptr<MWWorld::Action> ESM4Creature::activate(
        const MWWorld::Ptr& ptr, const MWWorld::Ptr& actor) const
    {
        const MWMechanics::CreatureStats& stats = getCreatureStats(ptr);
        if (stats.isDead())
            return std::make_unique<MWWorld::ActionOpen>(ptr);
        auto* world = dynamic_cast<MWWorld::World*>(
            static_cast<MWBase::World*>(MWBase::Environment::get().getWorld()));
        if (world != nullptr && world->getOblivionAiService() != nullptr
            && world->getOblivionAiService()->isHorse(ptr))
            return std::make_unique<MWWorld::ActionHorse>(ptr);
        if (!stats.getKnockedDown())
            return std::make_unique<MWWorld::ActionTalk>(ptr);
        return std::make_unique<MWWorld::FailedAction>();
    }

    ESM4Door::ESM4Door()
        : ESM4InteractiveBase(ESM4::Door::sRecordId)
    {
    }

    void ESM4Door::insertObject(const MWWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
        MWPhysics::PhysicsSystem& physics) const
    {
        insertObjectPhysics(ptr, model, rotation, physics);

        // A door can be paged out while an open/close transition is in
        // progress. Resume that transition when the native reference becomes
        // resident, exactly as the TES3 door class does.
        if (ptr.getRefData().getCustomData())
        {
            const ESM4DoorCustomData& customData
                = ptr.getRefData().getCustomData()->asESM4DoorCustomData();
            if (customData.mDoorState != MWWorld::DoorState::Idle)
                MWBase::Environment::get().getWorld()->activateDoor(ptr, customData.mDoorState);
        }
    }

    void ESM4Door::insertObjectPhysics(const MWWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
        MWPhysics::PhysicsSystem& physics) const
    {
        physics.addObject(ptr, VFS::Path::toNormalized(model), rotation, MWPhysics::CollisionType_Door);
    }

    void ESM4Door::ensureCustomData(const MWWorld::Ptr& ptr) const
    {
        if (!ptr.getRefData().getCustomData())
            ptr.getRefData().setCustomData(std::make_unique<ESM4DoorCustomData>());
    }

    MWWorld::DoorState ESM4Door::getDoorState(const MWWorld::ConstPtr& ptr) const
    {
        if (!ptr.getRefData().getCustomData())
            return MWWorld::DoorState::Idle;
        const ESM4DoorCustomData& customData = ptr.getRefData().getCustomData()->asESM4DoorCustomData();
        return customData.mDoorState;
    }

    void ESM4Door::setDoorState(const MWWorld::Ptr& ptr, MWWorld::DoorState state) const
    {
        if (ptr.getCellRef().getTeleport())
            throw std::runtime_error("load doors can't be moved");
        ensureCustomData(ptr);
        ESM4DoorCustomData& customData = ptr.getRefData().getCustomData()->asESM4DoorCustomData();
        customData.mDoorState = state;
    }

    void ESM4Door::readAdditionalState(const MWWorld::Ptr& ptr, const ESM::ObjectState& state) const
    {
        if (!state.mHasCustomState)
            return;
        ensureCustomData(ptr);
        ESM4DoorCustomData& customData = ptr.getRefData().getCustomData()->asESM4DoorCustomData();
        const ESM::DoorState& doorState = state.asDoorState();
        const int value = doorState.mDoorState;
        customData.mDoorState = value >= static_cast<int>(MWWorld::DoorState::Idle)
                && value <= static_cast<int>(MWWorld::DoorState::Closing)
            ? static_cast<MWWorld::DoorState>(value)
            : MWWorld::DoorState::Idle;
    }

    void ESM4Door::writeAdditionalState(const MWWorld::ConstPtr& ptr, ESM::ObjectState& state) const
    {
        if (!ptr.getRefData().getCustomData())
        {
            state.mHasCustomState = false;
            return;
        }

        const ESM4DoorCustomData& customData = ptr.getRefData().getCustomData()->asESM4DoorCustomData();
        ESM::DoorState& doorState = state.asDoorState();
        doorState.mDoorState = static_cast<int>(customData.mDoorState);
    }

    std::unique_ptr<MWWorld::Action> ESM4Door::activate(
        const MWWorld::Ptr& ptr, const MWWorld::Ptr& actor) const
    {
        const ESM4::Door* door = ptr.get<ESM4::Door>()->mBase;
        auto* world = static_cast<MWWorld::World*>(
            static_cast<MWBase::World*>(MWBase::Environment::get().getWorld()));
        if (!world->isOblivionDefaultActivation())
            return std::make_unique<MWWorld::OblivionInteractionAction>(
                ptr, MWWorld::OblivionInteractionKind::Door);
        if (ptr.getCellRef().isLocked())
        {
            const ESM::RefId key = ptr.getCellRef().getKey();
            bool hasKey = false;
            if (!actor.isEmpty())
                hasKey = !actor.getClass().getContainerStore(actor).search(key).isEmpty();
            // Keep the player fallback for projected/native inventory
            // identities. NPCs must pass through their own container above;
            // the default activation path must never unlock for them.
            if (!hasKey && actor == world->getPlayerPtr())
                hasKey = !key.empty() && world->oblivionPlayerHasItem(key);
            if (!key.empty() && hasKey)
            {
                ptr.getCellRef().unlock();
                if (actor == world->getPlayerPtr())
                    MWBase::Environment::get().getWindowManager()->messageBox("Key used");
            }
            else
            {
                Log(Debug::Info) << "M5 door activation: result=locked ref="
                                 << ptr.getCellRef().getFormKey().serialize() << " level="
                                 << ptr.getCellRef().getLockLevel();
                auto action = std::make_unique<MWWorld::FailedAction>(
                    "Locked (level " + std::to_string(std::max(0, ptr.getCellRef().getLockLevel())) + ")", ptr);
                return action;
            }
        }

        if (ptr.getCellRef().getTeleport())
        {
            Log(Debug::Info) << "M5 door activation: result=teleport ref="
                             << ptr.getCellRef().getFormKey().serialize() << " destination="
                             << ptr.getCellRef().getDestCell();
            auto action = std::make_unique<MWWorld::ActionTeleport>(
                ptr.getCellRef().getDestCell(), ptr.getCellRef().getDoorDest(), actor == world->getPlayerPtr());
            setActionSound(*action, door->mOpenSound);
            return action;
        }

        auto action = std::make_unique<MWWorld::ActionDoor>(ptr);
        Log(Debug::Info) << "M5 door activation: result=animated ref="
                         << ptr.getCellRef().getFormKey().serialize();
        const float current = ptr.getRefData().getPosition().rot[2];
        const float closed = ptr.getCellRef().getPosition().rot[2];
        setActionSound(*action, current == closed ? door->mOpenSound : door->mCloseSound);
        return action;
    }

    MWGui::ToolTipInfo ESM4Door::getToolTipInfo(const MWWorld::ConstPtr& ptr, int count) const
    {
        MWGui::ToolTipInfo info = ESM4InteractiveBase::getToolTipInfo(ptr, count);
        appendReferenceState(info, ptr);
        if (ptr.getCellRef().getTeleport())
        {
            const ESM::RefId cell = ptr.getCellRef().getDestCell();
            if (!cell.empty())
                info.text += "\nDoor to " + std::string(MWBase::Environment::get().getWorld()->getCellName(
                                               &MWBase::Environment::get().getWorldModel()->getCell(cell)));
        }
        return info;
    }

    ESM4Flora::ESM4Flora()
        : ESM4InteractiveBase(ESM4::Flora::sRecordId)
    {
    }

    std::unique_ptr<MWWorld::Action> ESM4Flora::activate(const MWWorld::Ptr& ptr, const MWWorld::Ptr&) const
    {
        auto action
            = std::make_unique<MWWorld::OblivionInteractionAction>(ptr, MWWorld::OblivionInteractionKind::Flora);
        if (ptr.getCellRef().getOwner().empty())
            setActionSound(*action, ptr.get<ESM4::Flora>()->mBase->mSound);
        return action;
    }
}
