#include "oblivionactorstats.hpp"

#include "esmstore.hpp"

#include <components/esm/records.hpp>
#include <components/esm4/combatsettings.hpp>

#include <set>
#include <stdexcept>
#include <vector>

namespace MWWorld
{
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
