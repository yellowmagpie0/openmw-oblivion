#ifndef GAME_MWCLASS_ESM4ACTOR_H
#define GAME_MWCLASS_ESM4ACTOR_H

#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadclot.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrace.hpp>
#include <components/esm4/playermechanics.hpp>

#include "../mwgui/tooltips.hpp"

#include "../mwmechanics/movement.hpp"
#include "../mwmechanics/npcstats.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/customdata.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/registeredclass.hpp"

#include "actor.hpp"
#include "esm4base.hpp"

namespace MWClass
{
    class ESM4NpcCustomData final : public MWWorld::TypedCustomData<ESM4NpcCustomData>
    {
    public:
        MWMechanics::NpcStats mNpcStats;
        MWMechanics::Movement mMovement;
        MWWorld::InventoryStore mInventoryStore;

        const ESM4::Npc* mTraits = nullptr;
        const ESM4::Npc* mBaseData = nullptr;
        const ESM4::Race* mRace = nullptr;
        bool mIsFemale = false;

        // These are a render-facing cache of the native records selected by
        // the live InventoryStore.  InventoryStore remains the authority for
        // counts, slots, and runtime changes.
        std::vector<const ESM4::Armor*> mEquippedArmor;
        std::vector<const ESM4::Clothing*> mEquippedClothing;

        ESM4NpcCustomData& asESM4NpcCustomData() override { return *this; }
        const ESM4NpcCustomData& asESM4NpcCustomData() const override { return *this; }
    };

    class ESM4Npc final : public MWWorld::RegisteredClass<ESM4Npc, Actor>
    {
        friend MWWorld::RegisteredClass<ESM4Npc, Actor>;

    public:
        ESM4Npc()
            : MWWorld::RegisteredClass<ESM4Npc, Actor>(ESM4::Npc::sRecordId)
        {
        }

        MWWorld::Ptr copyToCellImpl(const MWWorld::ConstPtr& ptr, MWWorld::CellStore& cell) const override
        {
            const MWWorld::LiveCellRef<ESM4::Npc>* ref = ptr.get<ESM4::Npc>();
            MWWorld::Ptr result(cell.insert(ref), &cell);
            if (result.getRefData().getCustomData())
                result.getClass().getInventoryStore(result).setPtr(result);
            return result;
        }

        void insertObjectRendering(const MWWorld::Ptr& ptr, const std::string& model,
            MWRender::RenderingInterface& renderingInterface) const override
        {
            renderingInterface.getObjects().insertNPC(ptr);
        }

        bool hasToolTip(const MWWorld::ConstPtr& ptr) const override { return true; }
        MWGui::ToolTipInfo getToolTipInfo(const MWWorld::ConstPtr& ptr, int count) const override
        {
            return ESM4Impl::getToolTipInfo(getName(ptr), count);
        }

        VFS::Path::NormalizedView getModel(const MWWorld::ConstPtr& ptr) const override;
        std::string_view getName(const MWWorld::ConstPtr& ptr) const override;
        MWMechanics::CreatureStats& getCreatureStats(const MWWorld::Ptr& ptr) const override;
        MWMechanics::NpcStats& getNpcStats(const MWWorld::Ptr& ptr) const override;
        MWWorld::ContainerStore& getContainerStore(const MWWorld::Ptr& ptr) const override;
        MWWorld::InventoryStore& getInventoryStore(const MWWorld::Ptr& ptr) const override;
        bool hasInventoryStore(const MWWorld::ConstPtr&) const override { return true; }
        ESM::RefId getScript(const MWWorld::ConstPtr& ptr) const override;
        float getCapacity(const MWWorld::Ptr& ptr) const override;
        float getArmorRating(const MWWorld::Ptr& ptr, bool useLuaInterfaceIfAvailable) const override;
        bool isEssential(const MWWorld::ConstPtr& ptr) const override;
        int getServices(const MWWorld::ConstPtr& ptr) const override;
        bool isPersistent(const MWWorld::ConstPtr& ptr) const override;
        MWMechanics::Movement& getMovementSettings(const MWWorld::Ptr& ptr) const override;
        float getMaxSpeed(const MWWorld::Ptr& ptr) const override;
        float getJump(const MWWorld::Ptr& ptr) const override;
        float getWalkSpeed(const MWWorld::Ptr& ptr) const override;
        float getRunSpeed(const MWWorld::Ptr& ptr) const override;
        float getSwimSpeed(const MWWorld::Ptr& ptr) const override;
        bool isBipedal(const MWWorld::ConstPtr&) const override { return true; }
        bool canFly(const MWWorld::ConstPtr&) const override { return false; }
        bool canSwim(const MWWorld::ConstPtr&) const override { return true; }
        bool canWalk(const MWWorld::ConstPtr&) const override { return true; }
        float getSkill(const MWWorld::Ptr& ptr, ESM::RefId id) const override;
        int getBaseFightRating(const MWWorld::ConstPtr& ptr) const override;
        ESM::RefId getPrimaryFaction(const MWWorld::ConstPtr& ptr) const override;
        int getPrimaryFactionRank(const MWWorld::ConstPtr& ptr) const override;
        std::unique_ptr<MWWorld::Action> activate(
            const MWWorld::Ptr& ptr, const MWWorld::Ptr& actor) const override;
        void getModelsToPreload(
            const MWWorld::ConstPtr& ptr, std::vector<VFS::Path::NormalizedView>& models) const override;
        bool useAnim() const override { return true; }
        bool isNpc() const override { return true; }
        void adjustScale(const MWWorld::ConstPtr& ptr, osg::Vec3f& scale, bool rendering) const override;

        static const ESM4::Npc* getTraitsRecord(const MWWorld::Ptr& ptr);
        static const ESM4::Race* getRace(const MWWorld::Ptr& ptr);
        static bool isFemale(const MWWorld::Ptr& ptr);
        static const std::vector<const ESM4::Armor*>& getEquippedArmor(const MWWorld::Ptr& ptr);
        static const std::vector<const ESM4::Clothing*>& getEquippedClothing(const MWWorld::Ptr& ptr);

    private:
        static ESM4NpcCustomData& getCustomData(const MWWorld::ConstPtr& ptr);
    };
}

#endif // GAME_MWCLASS_ESM4ACTOR_H
