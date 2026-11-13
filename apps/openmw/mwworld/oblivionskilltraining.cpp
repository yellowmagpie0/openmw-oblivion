#include "oblivionskilltraining.hpp"
#include "worldimp.hpp"
#include "class.hpp"
#include "player.hpp"
#include "oblivionactorstats.hpp"
#include "../mwmechanics/npcstats.hpp"
#include "../mwmechanics/oblivioncombat.hpp"
#include <components/esm4/loadclas.hpp>
#include <components/esm4/loadgmst.hpp>
#include <components/esm4/loadskil.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/playermiscstats.hpp>
#include <components/debug/debuglog.hpp>
#include <components/esm4/runtimestate.hpp>
#include <components/esm4/skilltraining.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>

namespace MWWorld
{
    namespace
    {
        struct SkillContext
        {
            const ESM4::Skill* mDefinition;
            ESM4::SkillTrainingSettings mSettings;
            bool mMajor, mSpecialized;
        };

        SkillContext skillContext(const ESMStore& store, const Ptr& player, unsigned skill)
        {
            if (skill < 12 || skill > 32 || player.isEmpty())
                throw std::invalid_argument("invalid native Player skill");
            const ESM4::Skill* definition = nullptr;
            for (const auto& entry : store.get<ESM4::Skill>())
            {
                const auto* winning = store.get<ESM4::Skill>().search(entry.mId);
                if (winning && winning->mData && winning->mData->mActorValue == skill)
                {
                    if (definition && definition != winning)
                        throw std::invalid_argument("ambiguous native skill definition");
                    definition = winning;
                }
            }
            if (!definition || definition->mData->mGoverningAttribute >= 8
                || definition->mData->mSpecialization >= 3)
                throw std::invalid_argument("native skill progression requires a valid winning skill definition");
            ESM4::SkillTrainingSettings settings;
            const std::array settingNames{"fSkillUseFactor", "fSkillUseExp", "fSkillUseMajorMult",
                "fSkillUseMinorMult", "fSkillUseSpecMult"};
            const std::array settingTargets{&settings.mFactor, &settings.mExponent, &settings.mMajorMultiplier,
                &settings.mMinorMultiplier, &settings.mSpecializationMultiplier};
            std::uint32_t temporaryClass = 143590;
            for (const auto& entry : store.get<ESM4::GameSetting>())
            {
                const auto* winning = store.get<ESM4::GameSetting>().search(entry.mId);
                if (!winning) continue;
                for (std::size_t i = 0; i < settingNames.size(); ++i)
                    if (Misc::StringUtils::ciEqual(winning->mEditorId, settingNames[i]))
                    {
                        const auto* value = std::get_if<float>(&winning->mData);
                        if (!value) throw std::invalid_argument("native skill experience setting must be float");
                        *settingTargets[i] = *value;
                    }
                if (Misc::StringUtils::ciEqual(winning->mEditorId, "iClassCharactergenClass"))
                {
                    const auto* value = std::get_if<std::int32_t>(&winning->mData);
                    if (!value) throw std::invalid_argument("native temporary class setting must be integer");
                    temporaryClass = std::bit_cast<std::uint32_t>(*value);
                }
            }
            const auto classId = player.get<ESM::NPC>()->mBase->mClass;
            const auto temporaryId = store.resolveEsm4RuntimeFormId(temporaryClass);
            bool major = false, specialized = false;
            if (!temporaryId || classId != ESM::RefId(*temporaryId))
            {
                if (const auto* characterClass = store.get<ESM4::Class>().search(classId))
                {
                    major = std::find(characterClass->mData.mMajorSkills.begin(),
                        characterClass->mData.mMajorSkills.end(), skill) != characterClass->mData.mMajorSkills.end();
                    specialized = characterClass->mData.mSpecialization == definition->mData->mSpecialization;
                }
                else
                {
                    const auto* custom = store.get<ESM::Class>().find(classId);
                    const auto& skillId = oblivionSkillIds()[skill - 12];
                    for (std::size_t i = 0; i < 5; ++i)
                        major |= custom->mData.mSkills[i][1] == skillId;
                    major |= custom->mData.mSkills[0][0] == skillId || custom->mData.mSkills[1][0] == skillId;
                    specialized = static_cast<std::uint32_t>(custom->mData.mSpecialization) == definition->mData->mSpecialization;
                }
            }
            return {definition, settings, major, specialized};
        }
    }

    float oblivionPlayerSkillProgressRequirement(const ESMStore& store, const Ptr& player, unsigned skill)
    {
        const auto context = skillContext(store, player, skill);
        const auto& stat = player.getClass().getNpcStats(player).getSkill(oblivionSkillIds()[skill - 12]);
        return ESM4::skillExperienceRequirement(stat.getBase(), context.mMajor, context.mSpecialized, context.mSettings);
    }

    void World::useOblivionPlayerSkill(unsigned skill, unsigned usage, float scale)
    {
        const auto context = skillContext(mStore, getPlayerPtr(), skill);
        if (usage >= 2 || !std::isfinite(scale) || scale < 0.f)
            throw std::invalid_argument("invalid native skill use");
        const float experience = context.mDefinition->mData->mUseValues[usage] * scale;
        updateOblivionPlayerSkill(skill, 0, experience);
    }

    bool World::readOblivionSkillBook(const Ptr& book)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || book.isEmpty()
            || (book.getType() != ESM::REC_BOOK && book.getType() != ESM::REC_BOOK4)) return false;
        const auto* definition = mStore.get<ESM4::Book>().search(book.getCellRef().getRefId());
        if (!definition || (definition->mFlags & ESM4::Rec_Deleted)
            || definition->mData.bookSkill < 0 || definition->mData.bookSkill > 20) return false;
        const auto key = mStore.get<ESM4::Book>().findFormKey(book.getCellRef().getRefId());
        if (!key || !key->isContent()) return false;
        if (const auto* winner = mStore.getFormKeyIndex().winner(*key);
            winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_BOOK)) return false;
        updateOblivionPlayerSkill(12 + definition->mData.bookSkill, 1, 0.f, *key);
        return true;
    }

    void validateOblivionAwardedSkillBooks(const ESMStore& store, const ESM4::RuntimeState& state)
    {
        for (const auto& key : state.mPlayer.mAwardedSkillBooks)
        {
            const auto* book = store.get<ESM4::Book>().search(key);
            const auto* winner = store.getFormKeyIndex().winner(key);
            if (!book || (book->mFlags & ESM4::Rec_Deleted) || book->mData.bookSkill < 0
                || book->mData.bookSkill > 20
                || (winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_BOOK)))
                throw std::invalid_argument("awarded native skill book no longer resolves");
        }
    }

    void World::updateOblivionPlayerSkill(unsigned skill, std::int32_t count, float experience,
        const ESM::FormKey& skillBook)
    {
        auto* combat = getOblivionCombatService();
        const auto player = getPlayerPtr();
        if (!combat || player.isEmpty() || !combat->findActorValues(ESM::FormKey::dynamic("player", 1)))
            throw std::invalid_argument("native skill progression requires Player actor values");
        const auto context = skillContext(mStore, player, skill);
        auto& stats = player.getClass().getNpcStats(player);
        auto& sharedSkill = stats.getSkill(oblivionSkillIds()[skill - 12]);
        const auto base = combat->getPlayerBaseValue(skill);
        const bool readingBook = !skillBook.isNull();
        if (readingBook && (!skillBook.isContent() || count != 1 || experience != 0.f))
            throw std::invalid_argument("invalid native skill-book reward request");
        const bool exhausted = readingBook && mOblivionRuntimeState
            && mOblivionRuntimeState->mPlayer.mAwardedSkillBooks.contains(skillBook);
        const auto training = readingBook
            ? exhausted ? ESM4::ScriptedSkillTraining{base, sharedSkill.getProgress()}
                        : ESM4::trainBookSkill(base, sharedSkill.getProgress())
            : count != 0
            ? ESM4::trainScriptedSkill(base, sharedSkill.getProgress(), count,
                context.mMajor, context.mSpecialized, context.mSettings)
            : ESM4::trainUsedSkill(base, sharedSkill.getProgress(), experience,
                context.mMajor, context.mSpecialized, context.mSettings);
        if (!readingBook && training.mBase == base && training.mExperience == sharedSkill.getProgress()) return;
        count = training.mBase - base;
        const std::array attributes{ESM::Attribute::Strength, ESM::Attribute::Intelligence, ESM::Attribute::Willpower,
            ESM::Attribute::Agility, ESM::Attribute::Speed, ESM::Attribute::Endurance,
            ESM::Attribute::Personality, ESM::Attribute::Luck};
        const auto attribute = attributes[context.mDefinition->mData->mGoverningAttribute];
        const auto specialization = static_cast<ESM::Class::Specialization>(context.mDefinition->mData->mSpecialization);
        const auto addCount = [count](int value) {
            return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value)
                + static_cast<std::uint32_t>(count));
        };
        const int attributeCount = addCount(stats.getSkillIncreasesForAttribute(attribute));
        const int majorCount = context.mMajor ? addCount(stats.getLevelProgress()) : stats.getLevelProgress();
        const auto specializationCount = static_cast<std::uint8_t>(
            static_cast<std::uint32_t>(stats.getSkillIncreasesForSpecialization(specialization))
            + static_cast<std::uint32_t>(count));
        if (!mOblivionRuntimeState)
            mOblivionRuntimeState = std::make_unique<ESM4::RuntimeState>(captureOblivionRuntimeState());
        auto counters = mOblivionRuntimeState->mPlayer.mActorValues;
        // Prepare all allocations before native skill publication. The history
        // is by book base, independently of the placed reference or container.
        std::set<ESM::FormKey> awardedBooks;
        if (readingBook)
        {
            awardedBooks = mOblivionRuntimeState->mPlayer.mAwardedSkillBooks;
            const auto increment = [&](int index) {
                auto& value = counters[ESM4::playerMiscStatKey(index)];
                value = ESM4::addPlayerMiscStat(ESM4::playerMiscStatValue(value), 1);
            };
            increment(17); // Every actual read, including repeats and skill100.
            if (count != 0)
            {
                awardedBooks.insert(skillBook);
                increment(18); // Learned skill books.
                increment(2);  // Actual skill increases, not scripted ModPCSkill.
            }
        }
        auto& advances = counters["skill_advances." + std::to_string(skill)];
        if (!std::isfinite(advances) || advances < 0 || advances > std::numeric_limits<std::uint32_t>::max()
            || std::trunc(advances) != advances)
            throw std::invalid_argument("invalid native per-skill advancement count");
        advances = static_cast<std::uint32_t>(advances) + static_cast<std::uint32_t>(count);
        if (count != 0 && !executeOblivionActorValueCommand(player, skill, ESM4::ActorValueCommand::Set,
                ESM4::ActorValueCommandSource::Script, training.mBase))
            throw std::invalid_argument("native skill training publication failed");
        stats.setSkillIncreasesForAttribute(attribute, attributeCount);
        stats.setSkillIncreasesForSpecialization(specialization, specializationCount);
        stats.setLevelProgress(majorCount);
        sharedSkill.setProgress(training.mExperience);
        mOblivionRuntimeState->mPlayer.mActorValues.swap(counters);
        if (readingBook)
        {
            mOblivionRuntimeState->mVersion = ESM4::CurrentRuntimeStateVersion;
            mOblivionRuntimeState->mPlayer.mAwardedSkillBooks.swap(awardedBooks);
            Log(Debug::Info) << "M16 skill book read: book=" << skillBook.serialize()
                << " skill=" << skill << " awarded=" << (count ? "true" : "false")
                << " base=" << training.mBase << " experience=" << training.mExperience;
        }
    }
}
