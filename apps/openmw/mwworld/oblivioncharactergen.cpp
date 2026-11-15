#include "oblivioncharactergen.hpp"
#include "worldimp.hpp"
#include "player.hpp"
#include "class.hpp"
#include "globals.hpp"
#include "oblivionprofileservices.hpp"
#include "../mwbase/environment.hpp"
#include "../mwmechanics/oblivioncombat.hpp"
#include <components/esm4/charactergen.hpp>
#include <components/esm4/loadclas.hpp>
#include <components/esm4/loadgmst.hpp>
#include <components/esm4/loadskil.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <bit>
#include <components/debug/debuglog.hpp>
#include <set>
#include <algorithm>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadskil.hpp>

namespace MWWorld
{
    namespace
    {
        ESM4::CharacterGenerationState currentState(World& world)
        {
            if (const auto& owned = world.getPlayer().getOblivionCharacterGenerationState()) return *owned;
            // Pre-v80 fork saves had no tutorial-use owner. Start at adoption;
            // never reconstruct historic uses from skill levels or XP progress.
            ESM4::CharacterGenerationState result;
            result.mTracking = world.getGlobalInt(Globals::sCharGenState) == 1;
            return result;
        }

        ESM::FormKey classSetting(World& world, std::string_view name, std::uint32_t fallback)
        {
            std::uint32_t id = fallback;
            std::set<ESM::FormId> seen;
            for (const auto& entry : world.getStore().get<ESM4::GameSetting>())
                if (seen.insert(entry.mId).second)
                {
                    const auto* setting = world.getStore().get<ESM4::GameSetting>().search(entry.mId);
                    if (setting && Misc::StringUtils::ciEqual(setting->mEditorId, name))
                    {
                        const auto* integer = std::get_if<std::int32_t>(&setting->mData);
                        if (!integer) throw std::invalid_argument("native class setting requires an integer");
                        id = std::bit_cast<std::uint32_t>(*integer);
                        break;
                    }
                }
            if (!id) return {};
            const auto managed = world.getStore().resolveEsm4RuntimeFormId(id);
            if (!managed) return {};
            return world.getStore().get<ESM4::Class>().findFormKey(ESM::RefId(*managed)).value_or(ESM::FormKey{});
        }

        std::array<const ESM4::Skill*, 21> skills(const ESMStore& store)
        {
            std::vector<const ESM4::Skill*> records;
            std::set<ESM::FormId> seen;
            for (const auto& entry : store.get<ESM4::Skill>())
                if (seen.insert(entry.mId).second) records.push_back(store.get<ESM4::Skill>().search(entry.mId));
            return ESM4::resolveSkillDefinitions(records);
        }
    }

    void setOblivionCharacterGenerationTracking(World& world, bool enabled)
    {
        world.getPlayer().setOblivionCharacterGenerationState(
            ESM4::setCharacterGenerationTracking(currentState(world), enabled));
    }

    static ESM::FormKey oblivionRecommendedClass(World& world)
    {
        const auto player = world.getPlayerPtr();
        if (player.isEmpty()) throw std::invalid_argument("native class query requires Player");
        const auto currentId = player.get<ESM::NPC>()->mBase->mClass;
        const auto currentKey = world.getStore().get<ESM4::Class>().findFormKey(currentId);
        const auto* currentNative = currentKey ? world.getStore().search<ESM4::Class>(*currentKey) : nullptr;
        const auto* currentShared = world.getStore().get<ESM::Class>().search(currentId);
        if (!currentNative && !currentShared) throw std::invalid_argument("native class query has no current class");
        auto state = currentState(world);
        const auto temporary = classSetting(world, "iClassCharactergenClass", 143590);
        if (currentKey && *currentKey == temporary)
        {
            if (!state.mSkillUses) throw std::invalid_argument("temporary class has no tutorial-use history");
            const auto definitions = skills(world.getStore());
            std::array<float, 3> totals{};
            for (std::size_t i = 0; i < definitions.size(); ++i)
                if ((*state.mSkillUses)[i] > 0)
                    totals[definitions[i]->mData->mSpecialization] += (*state.mSkillUses)[i];
            if (const auto proposal = ESM4::proposeCharacterClass(totals))
            {
                const auto key = classSetting(world, proposal->mSetting, proposal->mFormId);
                state.mRecommendedClass = world.getStore().search<ESM4::Class>(key) ? key : ESM::FormKey{};
            }
            if (state.mRecommendedClass.isNull())
            {
                const auto fallback = ESM4::DefaultCharacterGenerationClass;
                const auto key = classSetting(world, fallback.mSetting, fallback.mFormId);
                if (world.getStore().search<ESM4::Class>(key)) state.mRecommendedClass = key;
            }
        }
        if (!world.getPlayer().getOblivionCharacterGenerationState())
            Log(Debug::Info) << "Native chargen history adopted: tracking=" << state.mTracking;
        const auto result = state.mRecommendedClass;
        if (!result.isNull() && !world.getStore().search<ESM4::Class>(result))
            throw std::invalid_argument("native cached class is unresolved");
        world.getPlayer().setOblivionCharacterGenerationState(std::move(state));
        return result;
    }

    bool oblivionIsClassDefault(World& world, const ESM::FormKey& parameter)
    {
        // Original4F83D0 queries Player regardless of the command/condition subject.
        // A non-CLAS parameter becomes null before the identity comparison.
        const auto recommended = oblivionRecommendedClass(world);
        const auto key = world.getStore().search<ESM4::Class>(parameter) ? parameter : ESM::FormKey{};
        return recommended == key;
    }

    unsigned oblivionClassDefaultMatch(World& world)
    {
        const auto recommendedKey = oblivionRecommendedClass(world);
        const auto currentId = world.getPlayerPtr().get<ESM::NPC>()->mBase->mClass;
        const auto currentKey = world.getStore().get<ESM4::Class>().findFormKey(currentId);
        const auto* currentNative = currentKey ? world.getStore().search<ESM4::Class>(*currentKey) : nullptr;
        const auto* currentShared = world.getStore().get<ESM::Class>().search(currentId);
        const auto* recommended = world.getStore().search<ESM4::Class>(recommendedKey);
        const auto specialization = currentNative ? currentNative->mData.mSpecialization
                                                  : static_cast<std::uint32_t>(currentShared->mData.mSpecialization);
        return ESM4::characterClassDefaultMatch(currentKey && *currentKey == recommendedKey,
            recommended ? std::optional(recommended->mData.mSpecialization) : std::nullopt, specialization);
    }

    std::optional<ESM4::CharacterGenerationState> prepareOblivionCharacterGenerationSkillUse(
        World& world, unsigned skill, int usage, float factor)
    {
        auto state = currentState(world);
        if (!state.mTracking) return std::nullopt;
        if (skill < 12 || skill > 32 || usage < 0 || usage > 1 || !std::isfinite(factor) || factor < 0)
            throw std::invalid_argument("unsupported native tutorial skill-use event");
        const auto* combat = world.getOblivionCombatService();
        if (!combat) throw std::invalid_argument("tutorial skill use requires native Player values");
        if (combat->getPlayerBaseValue(skill) >= 100) return std::nullopt;
        const auto definitions = skills(world.getStore());
        const float amount = definitions[skill - 12]->mData->mUseValues[usage] * factor;
        return ESM4::recordCharacterGenerationSkillUse(std::move(state), skill, amount);
    }

    void recordOblivionCharacterGenerationSkillUse(const Ptr& actor, ESM::RefId skill, int usage, float factor)
    {
        auto* world = dynamic_cast<World*>(static_cast<MWBase::World*>(MWBase::Environment::get().getWorld()));
        if (!world || world->getGameProfile() != ESM::GameProfile::Oblivion || actor != world->getPlayerPtr()) return;
        static const std::array ids{ESM::Skill::Armorer, ESM::Skill::Athletics, ESM::Skill::LongBlade, ESM::Skill::Block,
            ESM::Skill::BluntWeapon, ESM::Skill::HandToHand, ESM::Skill::HeavyArmor, ESM::Skill::Alchemy,
            ESM::Skill::Alteration, ESM::Skill::Conjuration, ESM::Skill::Destruction, ESM::Skill::Illusion,
            ESM::Skill::Mysticism, ESM::Skill::Restoration, ESM::Skill::Acrobatics, ESM::Skill::LightArmor,
            ESM::Skill::Marksman, ESM::Skill::Mercantile, ESM::Skill::Security, ESM::Skill::Sneak, ESM::Skill::Speechcraft};
        const auto found = std::find(ids.begin(), ids.end(), skill);
        if (found == ids.end()) return; // A TES3-only skill has no native use slot.
        if (const auto next = prepareOblivionCharacterGenerationSkillUse(*world,
                12 + static_cast<unsigned>(found - ids.begin()), usage, factor))
            world->getPlayer().setOblivionCharacterGenerationState(*next);
    }
}
