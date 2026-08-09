#include "esm4interactive.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <components/debug/debuglog.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwphysics/physicssystem.hpp"

#include "../mwworld/actiondoor.hpp"
#include "../mwworld/actionteleport.hpp"
#include "../mwworld/failedaction.hpp"
#include "../mwworld/worldimp.hpp"

namespace MWClass
{
    namespace
    {
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
        if (ptr.getCellRef().getEditorId() == "CGPrisonSecretWallRef")
            return "Loose prison wall";
        if (ptr.getCellRef().getEditorId() == "CGPrisonWallSwitchRef")
            return "Prison wall switch";
        return {};
    }

    void ESM4Activator::insertObjectPhysics(const MWWorld::Ptr& ptr, const std::string& model,
        const osg::Quat& rotation, MWPhysics::PhysicsSystem& physics) const
    {
        // Oblivion drives the tutorial wall's collision through its script. Its NIF is marked as visual-only,
        // which is correct after the scripted wall opens but must not make the closed wall permeable.
        const bool isTutorialWall = ptr.getCellRef().getEditorId() == "CGPrisonSecretWallRef";
        physics.addObject(ptr, VFS::Path::toNormalized(model), rotation, MWPhysics::CollisionType_World,
            !isTutorialWall);
        if (isTutorialWall)
            Log(Debug::Info) << "M5 collision wall: physics=" << (physics.getObject(ptr) ? "present" : "missing")
                             << " ref=" << ptr.getCellRef().getFormKey().serialize();
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
        : ESM4InteractiveBase(ESM4::Creature::sRecordId)
    {
    }

    std::unique_ptr<MWWorld::Action> ESM4Creature::activate(
        const MWWorld::Ptr& ptr, const MWWorld::Ptr&) const
    {
        return std::make_unique<MWWorld::OblivionInteractionAction>(
            ptr, MWWorld::OblivionInteractionKind::Container);
    }

    ESM4Door::ESM4Door()
        : ESM4InteractiveBase(ESM4::Door::sRecordId)
    {
    }

    void ESM4Door::insertObjectPhysics(const MWWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
        MWPhysics::PhysicsSystem& physics) const
    {
        physics.addObject(ptr, VFS::Path::toNormalized(model), rotation, MWPhysics::CollisionType_Door);
    }

    MWWorld::DoorState ESM4Door::getDoorState(const MWWorld::ConstPtr&) const
    {
        // World owns the active transition and the settled angle is stored in RefData.
        return MWWorld::DoorState::Idle;
    }

    void ESM4Door::setDoorState(const MWWorld::Ptr&, MWWorld::DoorState) const
    {
    }

    std::unique_ptr<MWWorld::Action> ESM4Door::activate(
        const MWWorld::Ptr& ptr, const MWWorld::Ptr& actor) const
    {
        const ESM4::Door* door = ptr.get<ESM4::Door>()->mBase;
        if (ptr.getCellRef().isLocked())
        {
            const ESM::RefId key = ptr.getCellRef().getKey();
            auto* world = static_cast<MWWorld::World*>(
                static_cast<MWBase::World*>(MWBase::Environment::get().getWorld()));
            if (!key.empty() && world->oblivionPlayerHasItem(key))
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
                ptr.getCellRef().getDestCell(), ptr.getCellRef().getDoorDest(), true);
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
