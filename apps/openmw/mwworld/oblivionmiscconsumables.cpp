#include "oblivionmiscconsumables.hpp"

#include "action.hpp"
#include "class.hpp"
#include "esmstore.hpp"
#include "inventorystore.hpp"
#include "oblivionactorstats.hpp"
#include "worldimp.hpp"
#include "worldmodel.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/oblivioncombat.hpp"

#include <components/debug/debuglog.hpp>
#include <components/esm3/loadgmst.hpp>
#include <components/esm4/actorvalues.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/runtimestate.hpp>

#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace MWWorld
{
    bool isOblivionWelkyndStone(const ESMStore& store, const ESM4::MiscItem& item)
    {
        if ((item.mFlags & ESM4::Rec_Deleted) || !item.mScriptId.isZeroOrUnset()) return false;
        const auto key = store.get<ESM4::MiscItem>().findFormKey(ESM::RefId(item.mId));
        if (!key || *key != ESM::FormKey::content("Oblivion.esm", 0x191)) return false;
        if (const auto* winner = store.getFormKeyIndex().winner(*key);
            winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_MISC)) return false;
        return true;
    }

    bool isOblivionWelkyndItem(MWBase::World& world, const ConstPtr& item)
    {
        if (world.getGameProfile() != ESM::GameProfile::Oblivion || item.isEmpty()) return false;
        const auto* native = world.getStore().get<ESM4::MiscItem>().search(item.getCellRef().getRefId());
        return native && isOblivionWelkyndStone(world.getStore(), *native);
    }

    namespace
    {
        std::string message(const ESMStore& store, std::string_view key, std::string_view fallback)
        {
            if (const auto* setting = store.get<ESM::GameSetting>().search(key);
                setting && setting->mValue.getType() == ESM::VT_String) return setting->mValue.getString();
            return std::string(fallback);
        }

        class WelkyndUse final : public Action
        {
        public:
            explicit WelkyndUse(const Ptr& item) : Action(false, item) {}

        private:
            void executeImp(const Ptr& actor) override
            {
                auto& world = *MWBase::Environment::get().getWorld();
                auto* native = dynamic_cast<World*>(&world);
                auto* combat = native ? native->getOblivionCombatService() : nullptr;
                const auto& item = getTarget();
                if (!combat || actor.isEmpty() || actor != world.getPlayerPtr()
                    || !isOblivionWelkyndItem(world, item) || item.getCellRef().getCount() < 1
                    || item.getCellRef().getNativeBoundSource()
                    || actor.getClass().getCreatureStats(actor).isDead()) return;
                auto& inventory = actor.getClass().getInventoryStore(actor);
                if (item.getContainerStore() != &inventory) return;
                const auto key = ESM::FormKey::dynamic("player", 1);
                const auto* before = combat->findActorValues(key);
                const auto* life = combat->findActorLife(key);
                if (!before || !life || life->mPhase != ESM4::ActorLifePhase::Alive) return;

                const float current = combat->getPlayerValue(9);
                const float maximum = ESM4::dynamicActorValueMaximum(combat->getPlayerBaseValue(9),
                    before->mValues[9].mModifiers[0].value_or(0.f), before->mOwner, before->mProcess);
                const auto& store = world.getStore();
                if (current >= maximum)
                {
                    Log(Debug::Info) << "M16 Welkynd rejected: item=content:oblivion.esm:000191"
                        << " reason=magicka-full current=" << current << " maximum=" << maximum;
                    MWBase::Environment::get().getWindowManager()->messageBox(message(store, "sMagicAlreadyFull",
                        "Because your Magicka is at maximum, the Welkynd Stone remains charged."));
                    return;
                }
                const float restoration = maximum - current;
                if (!std::isfinite(restoration)) throw std::invalid_argument("invalid native Welkynd restoration");
                auto candidate = *before;
                candidate.mValues[9] = ESM4::changeActorValueModifier(candidate.mValues[9], candidate.mOwner,
                    9, ESM4::ActorValueModifier::Damage, restoration);
                candidate.validate();
                const auto settings = resolveOblivionPlayerDynamicBaseSettings(store);
                auto debit = inventory.prepareItemRemoval(item, 1);
                if (debit->getCount() != 1 || !debit->isValid()) return;
                MWBase::Environment::get().getWorldModel()->registerPtr(item);
                combat->publishPlayerValues(world.getPlayer(), std::move(candidate), settings);
                if (!debit->commit()) throw std::logic_error("native Welkynd debit changed during publication");
                Log(Debug::Info) << "M16 Welkynd consumed: item=content:oblivion.esm:000191"
                    << " before=" << current << " maximum=" << maximum << " restored=" << restoration << " count=1";
                MWBase::Environment::get().getLuaManager()->itemConsumed(item, actor);
                debit->notify();
                MWBase::Environment::get().getWindowManager()->messageBox(
                    message(store, "sWelkyndStoneMessage", "Magicka restored."));
            }
        };
    }

    std::unique_ptr<Action> createOblivionMiscConsumableUse(MWBase::World& world, const Ptr& item)
    {
        if (!isOblivionWelkyndItem(world, item)) return {};
        return std::make_unique<WelkyndUse>(item);
    }
}
