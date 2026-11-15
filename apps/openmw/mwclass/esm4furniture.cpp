#include "esm4furniture.hpp"

#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwgui/mode.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwworld/action.hpp"
#include "../mwworld/nullaction.hpp"

#include <components/debug/debuglog.hpp>

namespace MWClass
{
    namespace
    {
        constexpr std::uint32_t SleepFurniture = 0x80000000u;

        class BedRest final : public MWWorld::Action
        {
        public:
            explicit BedRest(const MWWorld::Ptr& bed) : Action(false, bed) {}

        private:
            void executeImp(const MWWorld::Ptr& actor) override
            {
                const auto& bed = getTarget();
                auto& world = *MWBase::Environment::get().getWorld();
                // Jail sentences have their own transition owner; this action
                // admits ordinary bed rest only.
                if (world.getGameProfile() != ESM::GameProfile::Oblivion || world.isPlayerInJail()
                    || actor.isEmpty() || actor != world.getPlayerPtr()
                    || bed.isEmpty() || !bed.isInCell() || !bed.getRefData().isEnabled()
                    || bed.mRef->isDeleted() || actor.getClass().getCreatureStats(actor).isDead()
                    || !(bed.get<ESM4::Furniture>()->mBase->mActiveMarkerFlags & SleepFurniture))
                    return;

                // Use the same ownership, crime and danger authority as bed
                // scripts. Passing the actual bed selects sleep, not waiting.
                if (MWBase::Environment::get().getMechanicsManager()->sleepInBed(actor, bed))
                    return;
                const auto windows = MWBase::Environment::get().getWindowManager();
                windows->pushGuiMode(MWGui::GM_Rest, bed);
                Log(Debug::Info) << "M13 native bed rest: ref=" << bed.getCellRef().getFormKey().serialize()
                                 << " opened=" << (windows->containsMode(MWGui::GM_Rest) ? "true" : "false");
            }
        };
    }

    ESM4Furniture::ESM4Furniture()
        : MWWorld::RegisteredClass<ESM4Furniture, ESM4Base<ESM4::Furniture>>(ESM4::Furniture::sRecordId)
    {
    }

    std::string_view ESM4Furniture::getName(const MWWorld::ConstPtr& ptr) const
    {
        return ptr.get<ESM4::Furniture>()->mBase->mFullName;
    }

    bool ESM4Furniture::hasToolTip(const MWWorld::ConstPtr& ptr) const
    {
        return !getName(ptr).empty();
    }

    MWGui::ToolTipInfo ESM4Furniture::getToolTipInfo(const MWWorld::ConstPtr& ptr, int count) const
    {
        return ESM4Impl::getToolTipInfo(getName(ptr), count);
    }

    std::unique_ptr<MWWorld::Action> ESM4Furniture::activate(
        const MWWorld::Ptr& ptr, const MWWorld::Ptr&) const
    {
        if (ptr.get<ESM4::Furniture>()->mBase->mActiveMarkerFlags & SleepFurniture)
            return std::make_unique<BedRest>(ptr);
        return std::make_unique<MWWorld::NullAction>();
    }
}
