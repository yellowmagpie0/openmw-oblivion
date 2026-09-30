#include "oblivionactorstats.hpp"

#include "../mwmechanics/oblivioncombat.hpp"

#include "esmstore.hpp"
#include "class.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwmechanics/creaturestats.hpp"

#include <components/esm/records.hpp>
#include <components/esm4/combatsettings.hpp>
#include <components/esm4/runtimereferences.hpp>

#include <limits>
#include <bit>
#include <set>
#include <stdexcept>
#include <vector>

namespace MWWorld
{
    const std::array<ESM::RefId, 21>& oblivionSkillIds()
    {
        // ESM::Skill IDs are dynamically initialized in loadskil.cpp.
        // Keep this table function-local so it cannot copy those IDs
        // during static initialization before their backing strings
        // exist.
        static const std::array<ESM::RefId, 21> ids = { ESM::Skill::Armorer, ESM::Skill::Athletics,
            ESM::Skill::LongBlade, ESM::Skill::Block, ESM::Skill::BluntWeapon, ESM::Skill::HandToHand,
            ESM::Skill::HeavyArmor, ESM::Skill::Alchemy, ESM::Skill::Alteration, ESM::Skill::Conjuration,
            ESM::Skill::Destruction, ESM::Skill::Illusion, ESM::Skill::Mysticism, ESM::Skill::Restoration,
            ESM::Skill::Acrobatics, ESM::Skill::LightArmor, ESM::Skill::Marksman, ESM::Skill::Mercantile,
            ESM::Skill::Security, ESM::Skill::Sneak, ESM::Skill::Speechcraft };
        return ids;
    }

    OblivionActorBaseStats resolveOblivionActorConstructionStats(const ESMStore& store,
        ESM::FormId actorBase, bool scaled)
    {
        auto key = store.get<ESM4::Npc>().findFormKey(ESM::RefId(actorBase));
        if (!key)
            key = store.get<ESM4::Creature>().findFormKey(ESM::RefId(actorBase));
        if (!key)
            throw std::runtime_error("Native actor construction lacks a stable base identity");
        std::optional<std::uint16_t> playerLevel;
        if (scaled)
        {
            const auto player = MWBase::Environment::get().getWorld()->getPlayerPtr();
            if (player.isEmpty())
                throw std::runtime_error("Native scaled actor construction requires the player");
            const int level = player.getClass().getCreatureStats(player).getLevel();
            if (level < 1 || level > std::numeric_limits<std::uint16_t>::max())
                throw std::runtime_error("Native actor construction has invalid player base level");
            playerLevel = static_cast<std::uint16_t>(level);
        }
        return resolveOblivionActorBaseStats(store, *key, playerLevel);
    }

    namespace
    {
        template <typename T> std::vector<const T*> winningRecords(const ESMStore& store)
        {
            const auto& records = store.get<T>();
            std::vector<const T*> result;
            std::set<ESM::FormId> seen;
            for (const auto& record : records)
                if (seen.insert(record.mId).second)
                    result.push_back(records.search(record.mId));
            return result;
        }

        std::array<std::uint8_t, 8> attributes(const ESM4::AttributeValues& value)
        {
            return {value.strength, value.intelligence, value.willpower, value.agility,
                value.speed, value.endurance, value.personality, value.luck};
        }

        std::int16_t level(const ESM4::ACBS_TES4& base, std::optional<std::uint16_t> playerLevel)
        {
            const auto result = ESM4::resolveActorLevel(base, playerLevel);
            if (result < 1)
                throw std::invalid_argument("unsupported nonpositive resolved actor level");
            return result;
        }

        OblivionActorBaseStats npcStats(const ESMStore& store, const ESM4::Npc& npc,
            std::optional<std::uint16_t> playerLevel)
        {
            if (!npc.mIsTES4)
                throw std::invalid_argument("actor is not a TES4 NPC");
            OblivionActorBaseStats result{};
            result.mLevel = level(npc.mBaseConfig.tes4, playerLevel);
            result.mAttributes = attributes(npc.mData.attribs);
            const auto& s = npc.mData.skills;
            result.mSkills = {s.armorer, s.athletics, s.blade, s.block, s.blunt, s.handToHand,
                s.heavyArmor, s.alchemy, s.alteration, s.conjuration, s.destruction, s.illusion,
                s.mysticism, s.restoration, s.acrobatics, s.lightArmor, s.marksman, s.mercantile,
                s.security, s.sneak, s.speechcraft};
            result.mHealth = npc.mData.health;
            result.mMagicka = npc.mBaseConfig.tes4.baseSpell;
            result.mFatigue = npc.mBaseConfig.tes4.fatigue;
            if (!(npc.mBaseConfig.tes4.flags & ESM4::Npc::TES4_AutoCalcStats))
                return result;
            const auto* race = store.get<ESM4::Race>().search(npc.mRace);
            const auto* characterClass = store.get<ESM4::Class>().search(npc.mClass);
            if (!race || !race->mTES4SkillBonuses || !characterClass)
                throw std::invalid_argument("missing native race/class auto-calculation input");
            const auto skills = ESM4::resolveSkillDefinitions(winningRecords<ESM4::Skill>(store));
            const auto settings = winningRecords<ESM4::GameSetting>(store);
            ESM4::NpcAutoStatsInput input{};
            input.mLevel = result.mLevel;
            input.mRaceAttributes = attributes(npc.mBaseConfig.tes4.flags & ESM4::Npc::TES4_Female
                    ? race->mAttribFemale : race->mAttribMale);
            input.mAuthoredPersonality = npc.mData.attribs.personality;
            input.mFavoredAttributes = characterClass->mData.mFavoredAttributes;
            input.mMajorSkills = characterClass->mData.mMajorSkills;
            input.mSpecialization = characterClass->mData.mSpecialization;
            for (std::size_t i = 0; i < skills.size(); ++i)
                input.mSkills[i] = {skills[i]->mData->mGoverningAttribute, skills[i]->mData->mSpecialization};
            for (std::size_t i = 0; i < input.mRaceBonuses.size(); ++i)
                input.mRaceBonuses[i] = {(*race->mTES4SkillBonuses)[i].mSkill, (*race->mTES4SkillBonuses)[i].mBonus};
            const auto calculated = ESM4::calculateNpcAutoStats(input, ESM4::buildNpcAutoStatsSettings(settings));
            result.mAttributes = calculated.mAttributes;
            result.mSkills = calculated.mSkills;
            const auto& a = result.mAttributes;
            const auto dynamic = ESM4::calculateNpcDynamicStats({a[0], a[1], a[2], a[3], a[5], result.mLevel,
                input.mFavoredAttributes[0] == 5 || input.mFavoredAttributes[1] == 5, input.mSpecialization},
                ESM4::buildNpcDynamicStatsSettings(settings));
            result.mHealth = dynamic.mHealth;
            result.mMagicka = dynamic.mMagicka;
            result.mFatigue = dynamic.mFatigue;
            return result;
        }
    }

    MWMechanics::OblivionFrameSettings resolveOblivionFrameSettings(const ESMStore& store)
    {
        const auto settings = winningRecords<ESM4::GameSetting>(store);
        return {{ESM4::buildMovementFatigueSettings(settings), ESM4::buildFatigueRegenerationSettings(settings),
                    ESM4::buildCombatMasterySettings(settings), ESM4::buildPlayerDynamicBaseSettings(settings)},
            ESM4::buildMagickaRegenerationSettings(settings)};
    }

    MWMechanics::OblivionFatigueSettings resolveOblivionFatigueSettings(const ESMStore& store)
    {
        const auto settings = winningRecords<ESM4::GameSetting>(store);
        return {ESM4::buildMovementFatigueSettings(settings), ESM4::buildFatigueRegenerationSettings(settings),
            ESM4::buildCombatMasterySettings(settings), ESM4::buildPlayerDynamicBaseSettings(settings)};
    }

    ESM4::FatigueRegenerationSettings resolveOblivionFatigueRegenerationSettings(const ESMStore& store)
    {
        return ESM4::buildFatigueRegenerationSettings(winningRecords<ESM4::GameSetting>(store));
    }

    ESM4::SwimBreathSettings resolveOblivionSwimBreathSettings(const ESMStore& store)
    {
        return ESM4::buildSwimBreathSettings(winningRecords<ESM4::GameSetting>(store));
    }

    ESM4::EssentialRecoverySettings resolveOblivionEssentialRecoverySettings(const ESMStore& store)
    {
        return ESM4::buildEssentialRecoverySettings(winningRecords<ESM4::GameSetting>(store));
    }

    ESM4::PlayerDynamicBaseSettings resolveOblivionPlayerDynamicBaseSettings(const ESMStore& store)
    {
        return ESM4::buildPlayerDynamicBaseSettings(winningRecords<ESM4::GameSetting>(store));
    }

    ESM4::RuntimeActorValues resolveOblivionInitialNonPlayerValues(const ESMStore& store,
        const ESM::FormKey& actor, const ESM::FormKey& actorBase,
        std::optional<std::uint16_t> playerLevel, ESM4::ActorValueProcess process)
    {
        if (actor.isNull() || ESM4::runtimeReferenceKey(actor) == ESM::FormKey::dynamic("player", 1))
            throw std::invalid_argument("native nonplayer construction requires a nonplayer actor identity");
        const auto* npc = store.search<ESM4::Npc>(actorBase);
        const auto* creature = store.search<ESM4::Creature>(actorBase);
        if ((!npc && !creature) || (npc && creature)
            || (npc && (!npc->mIsTES4 || npc->mFormKey != actorBase))
            || (creature && (!creature->mAttackReach || creature->mFormKey != actorBase)))
            throw std::invalid_argument("native actor construction has a missing or mismatched winning base");
        const auto stats = resolveOblivionActorBaseStats(store, actorBase, playerLevel);
        const auto& ai = npc ? npc->mAIData : creature->mAIData;
        ESM4::RuntimeActorValues result;
        result.mActor = actor;
        result.mBase = actorBase;
        result.mProcess = process;
        for (std::size_t i = 0; i < stats.mAttributes.size(); ++i)
            result.mValues[i].mBase = stats.mAttributes[i];
        result.mNonPlayerFormHealth = std::bit_cast<std::int32_t>(stats.mHealth);
        result.mValues[8].mBase = static_cast<float>(*result.mNonPlayerFormHealth);
        result.mValues[9].mBase = stats.mMagicka;
        result.mValues[10].mBase = stats.mFatigue;
        for (std::size_t i = 0; i < stats.mSkills.size(); ++i)
            result.mValues[12 + i].mBase = stats.mSkills[i];
        const std::array<std::uint8_t, 4> settings{ai.aggression, ai.confidence, ai.energyLevel, ai.responsibility};
        for (std::size_t i = 0; i < settings.size(); ++i)
            result.mValues[33 + i].mBase = settings[i];
        // Common Script and process Damage containers start with permanent
        // AV9/10 zero nodes. LowProcess has no Maximum container; do not
        // invent its slot presence when preparing fresh Low actors.
        for (const auto av : {9, 10})
            result.mValues[av].mModifiers = {
                process == ESM4::ActorValueProcess::Active ? std::optional(0.f) : std::nullopt, 0.f, 0.f};
        result.validate();
        return result;
    }

    OblivionActorBaseStats resolveOblivionActorBaseStats(const ESMStore& store,
        const ESM::FormKey& actorBase, std::optional<std::uint16_t> playerLevel)
    {
        try
        {
            if (actorBase.isNull())
                throw std::invalid_argument("null actor base");
            if (actorBase == ESM::FormKey::content("Oblivion.esm", 7))
                throw std::invalid_argument("player base requires the native player-stat path");
            if (const auto* npc = store.search<ESM4::Npc>(actorBase))
                return npcStats(store, *npc, playerLevel);
            const auto* creature = store.search<ESM4::Creature>(actorBase);
            if (!creature || !creature->mAttackReach)
                throw std::invalid_argument("missing or unsupported native actor base");
            OblivionActorBaseStats result{};
            result.mLevel = level(creature->mBaseConfig.tes4, playerLevel);
            result.mAttributes = attributes(creature->mData.attribs);
            const bool scaled = creature->mBaseConfig.tes4.flags & ESM4::Creature::TES4_PCLevelOffset;
            const auto settings = scaled ? ESM4::buildCreatureBaseStatsSettings(winningRecords<ESM4::GameSetting>(store))
                                         : ESM4::buildCreatureBaseStatsSettings({});
            const auto values = ESM4::calculateCreatureBaseStats({scaled, result.mLevel,
                creature->mData.combat, creature->mData.magic, creature->mData.stealth,
                creature->mData.health, creature->mBaseConfig.tes4.baseSpell,
                creature->mBaseConfig.tes4.fatigue, creature->mData.damage}, settings);
            for (std::size_t i = 0; i < result.mSkills.size(); ++i)
                result.mSkills[i] = i < 7 ? values.mCombat : i < 14 ? values.mMagic : values.mStealth;
            result.mHealth = values.mHealth;
            result.mMagicka = values.mMagicka;
            result.mFatigue = values.mFatigue;
            result.mNaturalDamage = values.mDamage;
            return result;
        }
        catch (const std::exception& error)
        {
            throw std::runtime_error("Native actor stats for " + actorBase.serialize() + ": " + error.what());
        }
    }
}
