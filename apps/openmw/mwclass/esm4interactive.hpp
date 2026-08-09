#ifndef GAME_MWCLASS_ESM4INTERACTIVE_H
#define GAME_MWCLASS_ESM4INTERACTIVE_H

#include <components/esm4/loadacti.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadcont.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loaddoor.hpp>
#include <components/esm4/loadflor.hpp>

#include "../mwworld/oblivioninteraction.hpp"
#include "esm4base.hpp"

namespace MWClass
{
    template <class Derived, class Record>
    class ESM4InteractiveBase : public MWWorld::RegisteredClass<Derived, ESM4Base<Record>>
    {
    protected:
        explicit ESM4InteractiveBase(unsigned type)
            : MWWorld::RegisteredClass<Derived, ESM4Base<Record>>(type)
        {
        }

    public:
        std::string_view getName(const MWWorld::ConstPtr& ptr) const override
        {
            return ptr.get<Record>()->mBase->mFullName;
        }

        MWGui::ToolTipInfo getToolTipInfo(const MWWorld::ConstPtr& ptr, int count) const override
        {
            return ESM4Impl::getToolTipInfo(getName(ptr), count);
        }

        bool hasToolTip(const MWWorld::ConstPtr& ptr) const override { return !getName(ptr).empty(); }
    };

    template <class Record>
    class ESM4Takeable final : public ESM4InteractiveBase<ESM4Takeable<Record>, Record>
    {
        friend MWWorld::RegisteredClass<ESM4Takeable<Record>, ESM4Base<Record>>;

        ESM4Takeable()
            : ESM4InteractiveBase<ESM4Takeable<Record>, Record>(Record::sRecordId)
        {
        }

    public:
        bool isItem(const MWWorld::ConstPtr&) const override { return true; }

        std::unique_ptr<MWWorld::Action> activate(
            const MWWorld::Ptr& ptr, const MWWorld::Ptr&) const override
        {
            auto action = std::make_unique<MWWorld::OblivionInteractionAction>(
                ptr, MWWorld::OblivionInteractionKind::Take);
            if constexpr (requires { ptr.get<Record>()->mBase->mPickUpSound; })
            {
                const ESM::FormId sound = ptr.get<Record>()->mBase->mPickUpSound;
                if (ptr.getCellRef().getOwner().empty() && !sound.isZeroOrUnset())
                    action->setSound(ESM::RefId(sound));
            }
            return action;
        }
    };

    class ESM4Activator final : public ESM4InteractiveBase<ESM4Activator, ESM4::Activator>
    {
        friend MWWorld::RegisteredClass<ESM4Activator, ESM4Base<ESM4::Activator>>;
        ESM4Activator();

    public:
        std::string_view getName(const MWWorld::ConstPtr& ptr) const override;
        void insertObjectPhysics(const MWWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            MWPhysics::PhysicsSystem& physics) const override;
        bool isActivator() const override { return true; }
        bool useAnim() const override { return true; }
        std::unique_ptr<MWWorld::Action> activate(
            const MWWorld::Ptr& ptr, const MWWorld::Ptr& actor) const override;
    };

    class ESM4Book final : public ESM4InteractiveBase<ESM4Book, ESM4::Book>
    {
        friend MWWorld::RegisteredClass<ESM4Book, ESM4Base<ESM4::Book>>;
        ESM4Book();

    public:
        bool isItem(const MWWorld::ConstPtr&) const override { return true; }
        std::unique_ptr<MWWorld::Action> activate(
            const MWWorld::Ptr& ptr, const MWWorld::Ptr& actor) const override;
    };

    class ESM4Container final : public ESM4InteractiveBase<ESM4Container, ESM4::Container>
    {
        friend MWWorld::RegisteredClass<ESM4Container, ESM4Base<ESM4::Container>>;
        ESM4Container();

    public:
        bool useAnim() const override { return true; }
        bool canLock(const MWWorld::ConstPtr&) const override { return true; }
        std::unique_ptr<MWWorld::Action> activate(
            const MWWorld::Ptr& ptr, const MWWorld::Ptr& actor) const override;
        MWGui::ToolTipInfo getToolTipInfo(const MWWorld::ConstPtr& ptr, int count) const override;
    };

    // Creatures are deliberately non-acting visual placeholders until the actor milestones. They still need to
    // expose their native inventory so the prison key carried by the tutorial goblin can be recovered.
    class ESM4Creature final : public ESM4InteractiveBase<ESM4Creature, ESM4::Creature>
    {
        friend MWWorld::RegisteredClass<ESM4Creature, ESM4Base<ESM4::Creature>>;
        ESM4Creature();

    public:
        std::unique_ptr<MWWorld::Action> activate(
            const MWWorld::Ptr& ptr, const MWWorld::Ptr& actor) const override;
    };

    class ESM4Door final : public ESM4InteractiveBase<ESM4Door, ESM4::Door>
    {
        friend MWWorld::RegisteredClass<ESM4Door, ESM4Base<ESM4::Door>>;
        ESM4Door();

    public:
        void insertObjectPhysics(const MWWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            MWPhysics::PhysicsSystem& physics) const override;
        bool isDoor() const override { return true; }
        bool useAnim() const override { return true; }
        bool canLock(const MWWorld::ConstPtr&) const override { return true; }
        MWWorld::DoorState getDoorState(const MWWorld::ConstPtr& ptr) const override;
        void setDoorState(const MWWorld::Ptr& ptr, MWWorld::DoorState state) const override;
        std::unique_ptr<MWWorld::Action> activate(
            const MWWorld::Ptr& ptr, const MWWorld::Ptr& actor) const override;
        MWGui::ToolTipInfo getToolTipInfo(const MWWorld::ConstPtr& ptr, int count) const override;
    };

    class ESM4Flora final : public ESM4InteractiveBase<ESM4Flora, ESM4::Flora>
    {
        friend MWWorld::RegisteredClass<ESM4Flora, ESM4Base<ESM4::Flora>>;
        ESM4Flora();

    public:
        std::unique_ptr<MWWorld::Action> activate(
            const MWWorld::Ptr& ptr, const MWWorld::Ptr& actor) const override;
    };
}

#endif
