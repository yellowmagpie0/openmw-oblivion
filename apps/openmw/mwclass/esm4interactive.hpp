#ifndef GAME_MWCLASS_ESM4INTERACTIVE_H
#define GAME_MWCLASS_ESM4INTERACTIVE_H

#include <array>
#include <optional>
#include <type_traits>

#include <components/esm4/loadacti.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadclot.hpp>
#include <components/esm4/loadcont.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loaddoor.hpp>
#include <components/esm4/loadflor.hpp>

#include "../mwworld/oblivioninteraction.hpp"
#include "../mwworld/customdata.hpp"
#include "../mwworld/doorstate.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwmechanics/movement.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwclass/actor.hpp"
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

        std::pair<std::vector<int>, bool> getEquipmentSlots(const MWWorld::ConstPtr& ptr) const override
        {
            std::vector<int> slots;
            if constexpr (std::is_same_v<Record, ESM4::Armor>)
            {
                const std::uint32_t flags = ptr.get<Record>()->mBase->mArmorFlags;
                if (flags & ESM4::Armor::TES4_Head)
                    slots.push_back(MWWorld::InventoryStore::Slot_Helmet);
                else if (flags & ESM4::Armor::TES4_UpperBody)
                    slots.push_back(MWWorld::InventoryStore::Slot_Cuirass);
                else if (flags & ESM4::Armor::TES4_LowerBody)
                    slots.push_back(MWWorld::InventoryStore::Slot_Greaves);
                else if (flags & ESM4::Armor::TES4_Hands)
                    slots.push_back(MWWorld::InventoryStore::Slot_LeftGauntlet);
                else if (flags & ESM4::Armor::TES4_Feet)
                    slots.push_back(MWWorld::InventoryStore::Slot_Boots);
                else if (flags & ESM4::Armor::TES4_RightRing)
                {
                    slots.push_back(MWWorld::InventoryStore::Slot_RightRing);
                    if (flags & ESM4::Armor::TES4_LeftRing)
                        slots.push_back(MWWorld::InventoryStore::Slot_LeftRing);
                }
                else if (flags & ESM4::Armor::TES4_LeftRing)
                    slots.push_back(MWWorld::InventoryStore::Slot_LeftRing);
                else if (flags & ESM4::Armor::TES4_Amulet)
                    slots.push_back(MWWorld::InventoryStore::Slot_Amulet);
                else if (flags & ESM4::Armor::TES4_Shield)
                    slots.push_back(MWWorld::InventoryStore::Slot_CarriedLeft);
            }
            else if constexpr (std::is_same_v<Record, ESM4::Clothing>)
            {
                const std::uint32_t flags = ptr.get<Record>()->mBase->mClothingFlags;
                if (flags & ESM4::Armor::TES4_Head)
                    slots.push_back(MWWorld::InventoryStore::Slot_Helmet);
                else if (flags & ESM4::Armor::TES4_UpperBody)
                    slots.push_back(MWWorld::InventoryStore::Slot_Shirt);
                else if (flags & ESM4::Armor::TES4_LowerBody)
                    slots.push_back(MWWorld::InventoryStore::Slot_Pants);
                else if (flags & ESM4::Armor::TES4_Hands)
                    slots.push_back(MWWorld::InventoryStore::Slot_LeftGauntlet);
                else if (flags & ESM4::Armor::TES4_Feet)
                    slots.push_back(MWWorld::InventoryStore::Slot_Boots);
                else if (flags & ESM4::Armor::TES4_RightRing)
                {
                    slots.push_back(MWWorld::InventoryStore::Slot_RightRing);
                    if (flags & ESM4::Armor::TES4_LeftRing)
                        slots.push_back(MWWorld::InventoryStore::Slot_LeftRing);
                }
                else if (flags & ESM4::Armor::TES4_LeftRing)
                    slots.push_back(MWWorld::InventoryStore::Slot_LeftRing);
                else if (flags & ESM4::Armor::TES4_Amulet)
                    slots.push_back(MWWorld::InventoryStore::Slot_Amulet);
            }
            return { std::move(slots), false };
        }

        float getArmorRating(const MWWorld::Ptr& ptr, bool) const override
        {
            if constexpr (std::is_same_v<Record, ESM4::Armor>)
                return ptr.get<Record>()->mBase->mData.armor;
            return 0.f;
        }

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

    private:
        void ensureCustomData(const MWWorld::Ptr& ptr) const;
    };

    class ESM4CreatureCustomData final : public MWWorld::TypedCustomData<ESM4CreatureCustomData>
    {
    public:
        MWMechanics::CreatureStats mCreatureStats;
        std::optional<std::array<std::uint8_t, 21>> mNativeSkills;
        std::optional<std::uint16_t> mNativeDamage;
        MWMechanics::Movement mMovement;
        MWWorld::InventoryStore mInventoryStore;

        ESM4CreatureCustomData& asESM4CreatureCustomData() override { return *this; }
        const ESM4CreatureCustomData& asESM4CreatureCustomData() const override { return *this; }
    };

    class ESM4Creature final : public MWWorld::RegisteredClass<ESM4Creature, Actor>
    {
        friend MWWorld::RegisteredClass<ESM4Creature, Actor>;
        ESM4Creature();

    public:
        std::string_view getName(const MWWorld::ConstPtr& ptr) const override;
        MWGui::ToolTipInfo getToolTipInfo(const MWWorld::ConstPtr& ptr, int count) const override;
        bool hasToolTip(const MWWorld::ConstPtr& ptr) const override { return !getName(ptr).empty(); }
        MWMechanics::CreatureStats& getCreatureStats(const MWWorld::Ptr& ptr) const override;
        MWWorld::ContainerStore& getContainerStore(const MWWorld::Ptr& ptr) const override;
        MWWorld::InventoryStore& getInventoryStore(const MWWorld::Ptr& ptr) const override;
        bool hasInventoryStore(const MWWorld::ConstPtr&) const override { return true; }
        ESM::RefId getScript(const MWWorld::ConstPtr& ptr) const override;
        float getCapacity(const MWWorld::Ptr& ptr) const override;
        float getArmorRating(const MWWorld::Ptr& ptr, bool useLuaInterfaceIfAvailable) const override;
        bool isEssential(const MWWorld::ConstPtr& ptr) const override;
        bool isPersistent(const MWWorld::ConstPtr& ptr) const override;
        int getServices(const MWWorld::ConstPtr& ptr) const override;
        MWMechanics::Movement& getMovementSettings(const MWWorld::Ptr& ptr) const override;
        float getMaxSpeed(const MWWorld::Ptr& ptr) const override;
        float getJump(const MWWorld::Ptr& ptr) const override;
        float getWalkSpeed(const MWWorld::Ptr& ptr) const override;
        float getRunSpeed(const MWWorld::Ptr& ptr) const override;
        float getSwimSpeed(const MWWorld::Ptr& ptr) const override;
        bool isBipedal(const MWWorld::ConstPtr& ptr) const override;
        bool canFly(const MWWorld::ConstPtr& ptr) const override;
        bool canSwim(const MWWorld::ConstPtr& ptr) const override;
        bool canWalk(const MWWorld::ConstPtr& ptr) const override;
        int getBaseFightRating(const MWWorld::ConstPtr& ptr) const override;
        ESM::RefId getPrimaryFaction(const MWWorld::ConstPtr& ptr) const override;
        int getPrimaryFactionRank(const MWWorld::ConstPtr& ptr) const override;
        float getSkill(const MWWorld::Ptr& ptr, ESM::RefId id) const override;
        VFS::Path::NormalizedView getModel(const MWWorld::ConstPtr& ptr) const override;
        void adjustScale(const MWWorld::ConstPtr& ptr, osg::Vec3f& scale, bool rendering) const override;
        bool useAnim() const override { return true; }
        void insertObjectRendering(const MWWorld::Ptr& ptr, const std::string& model,
            MWRender::RenderingInterface& renderingInterface) const override;
        std::unique_ptr<MWWorld::Action> activate(
            const MWWorld::Ptr& ptr, const MWWorld::Ptr& actor) const override;

    private:
        void ensureCustomData(const MWWorld::Ptr& ptr) const;
        MWWorld::Ptr copyToCellImpl(const MWWorld::ConstPtr& ptr, MWWorld::CellStore& cell) const override;
    };

    class ESM4DoorCustomData final : public MWWorld::TypedCustomData<ESM4DoorCustomData>
    {
    public:
        MWWorld::DoorState mDoorState = MWWorld::DoorState::Idle;

        ESM4DoorCustomData& asESM4DoorCustomData() override { return *this; }
        const ESM4DoorCustomData& asESM4DoorCustomData() const override { return *this; }
    };

    class ESM4Door final : public ESM4InteractiveBase<ESM4Door, ESM4::Door>
    {
        friend MWWorld::RegisteredClass<ESM4Door, ESM4Base<ESM4::Door>>;
        ESM4Door();

    public:
        void insertObject(const MWWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            MWPhysics::PhysicsSystem& physics) const override;
        void insertObjectPhysics(const MWWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            MWPhysics::PhysicsSystem& physics) const override;
        bool isDoor() const override { return true; }
        bool useAnim() const override { return true; }
        bool canLock(const MWWorld::ConstPtr&) const override { return true; }
        MWWorld::DoorState getDoorState(const MWWorld::ConstPtr& ptr) const override;
        void setDoorState(const MWWorld::Ptr& ptr, MWWorld::DoorState state) const override;
        void readAdditionalState(const MWWorld::Ptr& ptr, const ESM::ObjectState& state) const override;
        void writeAdditionalState(const MWWorld::ConstPtr& ptr, ESM::ObjectState& state) const override;
        std::unique_ptr<MWWorld::Action> activate(
            const MWWorld::Ptr& ptr, const MWWorld::Ptr& actor) const override;
        MWGui::ToolTipInfo getToolTipInfo(const MWWorld::ConstPtr& ptr, int count) const override;

    private:
        void ensureCustomData(const MWWorld::Ptr& ptr) const;
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
