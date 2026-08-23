#include "oblivionprofileservices.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <variant>

#include <components/debug/debuglog.hpp>
#include <components/esm/records.hpp>
#include <components/esm3/variant.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/settings/values.hpp>

#include "esmstore.hpp"
#include "globals.hpp"

namespace
{
    // This is a versioned allowlist, not a catch-all fallback. Every entry is
    // a shared OpenMW boot contract observed in the M3 interior/exterior
    // scenarios. An unlisted dependency fails at its call site and must be
    // reviewed before it can become part of the Oblivion profile.
    constexpr std::array sRuntimeSettingIds{
        std::string_view("fAthleticsRunBonus"),
        std::string_view("fBaseRunMultiplier"),
        std::string_view("fCombatArmorMinMult"),
        std::string_view("fEncumberedMoveEffect"),
        std::string_view("fEncumbranceStrMult"),
        std::string_view("fFatigueBase"),
        std::string_view("fFatigueMult"),
        std::string_view("fFatigueSneakBase"),
        std::string_view("fFatigueSneakMult"),
        std::string_view("fFatigueSwimRunBase"),
        std::string_view("fFatigueSwimRunMult"),
        std::string_view("fFatigueSwimWalkBase"),
        std::string_view("fFatigueSwimWalkMult"),
        std::string_view("fHoldBreathTime"),
        std::string_view("fJumpAcrobaticsBase"),
        std::string_view("fJumpAcroMultiplier"),
        std::string_view("fJumpEncumbranceBase"),
        std::string_view("fJumpEncumbranceMultiplier"),
        std::string_view("fJumpRunMultiplier"),
        std::string_view("fKnockDownMult"),
        std::string_view("fMagicStartIconBlink"),
        std::string_view("fMajorSkillBonus"),
        std::string_view("fMaxFlySpeed"),
        std::string_view("fMaxWalkSpeed"),
        std::string_view("fMessageTimePerChar"),
        std::string_view("fMinFlySpeed"),
        std::string_view("fMinorSkillBonus"),
        std::string_view("fMinWalkSpeed"),
        std::string_view("fMiscSkillBonus"),
        std::string_view("fPCbaseMagickaMult"),
        std::string_view("fSneakSpeedMultiplier"),
        std::string_view("fSpecialSkillBonus"),
        std::string_view("fStromWalkMult"),
        std::string_view("fSwimRunAthleticsMult"),
        std::string_view("fSwimRunBase"),
        std::string_view("fUnarmoredBase1"),
        std::string_view("fUnarmoredBase2"),
        std::string_view("fVanityDelay"),
        std::string_view("fWerewolfAcrobatics"),
        std::string_view("fWerewolfAgility"),
        std::string_view("fWerewolfAlchemy"),
        std::string_view("fWerewolfAlteration"),
        std::string_view("fWerewolfArmorer"),
        std::string_view("fWerewolfAthletics"),
        std::string_view("fWerewolfAxe"),
        std::string_view("fWerewolfBlock"),
        std::string_view("fWerewolfBluntweapon"),
        std::string_view("fWerewolfConjuration"),
        std::string_view("fWerewolfDestruction"),
        std::string_view("fWerewolfEnchant"),
        std::string_view("fWerewolfEndurance"),
        std::string_view("fWerewolfHandtohand"),
        std::string_view("fWerewolfHeavyarmor"),
        std::string_view("fWerewolfIllusion"),
        std::string_view("fWerewolfIntellegence"),
        std::string_view("fWerewolfLightarmor"),
        std::string_view("fWerewolfLongblade"),
        std::string_view("fWerewolfLuck"),
        std::string_view("fWerewolfMarksman"),
        std::string_view("fWerewolfMediumarmor"),
        std::string_view("fWerewolfMerchantile"),
        std::string_view("fWerewolfMysticism"),
        std::string_view("fWerewolfPersonality"),
        std::string_view("fWerewolfRestoration"),
        std::string_view("fWereWolfRunMult"),
        std::string_view("fWerewolfSecurity"),
        std::string_view("fWerewolfShortblade"),
        std::string_view("fWerewolfSneak"),
        std::string_view("fWerewolfSpear"),
        std::string_view("fWerewolfSpeechcraft"),
        std::string_view("fWerewolfSpeed"),
        std::string_view("fWerewolfStrength"),
        std::string_view("fWerewolfUnarmored"),
        std::string_view("fWerewolfWillpower"),
        std::string_view("i1stPersonSneakDelta"),
        std::string_view("iAutoSpellalterationMax"),
        std::string_view("iAutoSpellconjurationMax"),
        std::string_view("iAutoSpelldestructionMax"),
        std::string_view("iAutoSpellillusionMax"),
        std::string_view("iAutoSpellmysticismMax"),
        std::string_view("iAutoSpellrestorationMax"),
        std::string_view("iKnockDownOddsBase"),
        std::string_view("iKnockDownOddsMult"),
        std::string_view("iMaxActivateDist"),
        std::string_view("iMonthsToRespawn"),
        std::string_view("sAdmire"),
        std::string_view("sAgiDesc"),
        std::string_view("sAllTab"),
        std::string_view("sApparatus"),
        std::string_view("sApparelTab"),
        std::string_view("sArea"),
        std::string_view("sArmor"),
        std::string_view("sAttributeAgility"),
        std::string_view("sAttributeEndurance"),
        std::string_view("sAttributeIntelligence"),
        std::string_view("sAttributeLuck"),
        std::string_view("sAttributePersonality"),
        std::string_view("sAttributeSpeed"),
        std::string_view("sAttributeStrength"),
        std::string_view("sAttributeWillpower"),
        std::string_view("sBarterDialog7"),
        std::string_view("sBarterDialog8"),
        std::string_view("sBounty"),
        std::string_view("sBreath"),
        std::string_view("sBribe 10 Gold"),
        std::string_view("sBribe 100 Gold"),
        std::string_view("sBribe 1000 Gold"),
        std::string_view("sBuy"),
        std::string_view("sCastCost"),
        std::string_view("sCharges"),
        std::string_view("sClass"),
        std::string_view("sCreate"),
        std::string_view("sCreatedEffects"),
        std::string_view("sDefaultCellname"),
        std::string_view("sDelete"),
        std::string_view("sDisposeofCorpse"),
        std::string_view("sDuration"),
        std::string_view("sEditNote"),
        std::string_view("sEffects"),
        std::string_view("sEnchantmentMenu3"),
        std::string_view("sEnchantmentMenu4"),
        std::string_view("sEnchantmentMenu6"),
        std::string_view("sEndDesc"),
        std::string_view("sFatigue"),
        std::string_view("sGold"),
        std::string_view("sGoodbye"),
        std::string_view("sHealth"),
        std::string_view("sIngredients"),
        std::string_view("sInPrisonTitle"),
        std::string_view("sIntDesc"),
        std::string_view("sIntimidate"),
        std::string_view("sInventory"),
        std::string_view("sItem"),
        std::string_view("sLevel"),
        std::string_view("sLevelProgress"),
        std::string_view("sLucDesc"),
        std::string_view("sMagic"),
        std::string_view("sMagicEffects"),
        std::string_view("sMagicMenu"),
        std::string_view("sMagicTab"),
        std::string_view("sMagnitude"),
        std::string_view("sMap"),
        std::string_view("sMaxSale"),
        std::string_view("sMiscTab"),
        std::string_view("sName"),
        std::string_view("sPerDesc"),
        std::string_view("sPersuasionMenuTitle"),
        std::string_view("sQuality"),
        std::string_view("sQuickMenuInstruc"),
        std::string_view("sQuickMenuTitle"),
        std::string_view("sRace"),
        std::string_view("sRange"),
        std::string_view("sRangeTouch"),
        std::string_view("sRechargeEnchantment"),
        std::string_view("sRepairServiceTitle"),
        std::string_view("sReputation"),
        std::string_view("sRest"),
        std::string_view("sSchoolalteration"),
        std::string_view("sSchoolconjuration"),
        std::string_view("sSchooldestruction"),
        std::string_view("sSchoolillusion"),
        std::string_view("sSchoolmysticism"),
        std::string_view("sSchoolrestoration"),
        std::string_view("sServiceRepairTitle"),
        std::string_view("sServiceSpellsTitle"),
        std::string_view("sServiceTrainingTitle"),
        std::string_view("sServiceTravelTitle"),
        std::string_view("sSkillAcrobatics"),
        std::string_view("sSkillAlchemy"),
        std::string_view("sSkillAlteration"),
        std::string_view("sSkillArmorer"),
        std::string_view("sSkillAthletics"),
        std::string_view("sSkillAxe"),
        std::string_view("sSkillBlock"),
        std::string_view("sSkillBluntweapon"),
        std::string_view("sSkillClassMajor"),
        std::string_view("sSkillClassMinor"),
        std::string_view("sSkillClassMisc"),
        std::string_view("sSkillConjuration"),
        std::string_view("sSkillDestruction"),
        std::string_view("sSkillEnchant"),
        std::string_view("sSkillHandtohand"),
        std::string_view("sSkillHeavyarmor"),
        std::string_view("sSkillIllusion"),
        std::string_view("sSkillLightarmor"),
        std::string_view("sSkillLongblade"),
        std::string_view("sSkillMarksman"),
        std::string_view("sSkillMaxReached"),
        std::string_view("sSkillMediumarmor"),
        std::string_view("sSkillMercantile"),
        std::string_view("sSkillMysticism"),
        std::string_view("sSkillProgress"),
        std::string_view("sSkillRestoration"),
        std::string_view("sSkillSecurity"),
        std::string_view("sSkillShortblade"),
        std::string_view("sSkillSneak"),
        std::string_view("sSkillSpear"),
        std::string_view("sSkillSpeechcraft"),
        std::string_view("sSkillUnarmored"),
        std::string_view("sSoulGem"),
        std::string_view("sSpdDesc"),
        std::string_view("sSpellmakingMenu1"),
        std::string_view("sSpellServiceTitle"),
        std::string_view("sStats"),
        std::string_view("sStrDesc"),
        std::string_view("sTakeAll"),
        std::string_view("sTaunt"),
        std::string_view("sTrainingServiceTitle"),
        std::string_view("sTravelServiceTitle"),
        std::string_view("sUntilHealed"),
        std::string_view("sUses"),
        std::string_view("sWeaponTab"),
        std::string_view("sWilDesc"),
        std::string_view("sWorld"),
    };

    std::string makeLabel(std::string_view id)
    {
        if (!id.empty())
            id.remove_prefix(1);
        std::string result;
        result.reserve(id.size() + 8);
        for (std::size_t i = 0; i < id.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(id[i]);
            if (i != 0 && c >= 'A' && c <= 'Z' && id[i - 1] != ' ')
                result.push_back(' ');
            result.push_back(static_cast<char>(c));
        }
        return result;
    }

    std::optional<ESM::Variant> convertGameSetting(const ESM4::GameSetting::Data& value)
    {
        return std::visit(
            [](const auto& item) -> std::optional<ESM::Variant> {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, std::monostate>)
                    return std::nullopt;
                else if constexpr (std::is_same_v<T, std::string>)
                    return ESM::Variant(item);
                else if constexpr (std::is_same_v<T, float>)
                    return ESM::Variant(item);
                else if constexpr (std::is_same_v<T, bool>)
                    return ESM::Variant(static_cast<std::int32_t>(item));
                else if constexpr (std::is_same_v<T, std::uint32_t>)
                    return ESM::Variant(static_cast<std::int32_t>(
                        std::min(item, static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))));
                else
                    return ESM::Variant(static_cast<std::int32_t>(item));
            },
            value);
    }

    ESM::Variant convertGlobal(const ESM4::GlobalVariable& source)
    {
        ESM::Variant result;
        if (source.mType == 'f')
        {
            result.setType(ESM::VT_Float);
            result.setFloat(source.mValue);
        }
        else
        {
            result.setType(source.mType == 'l' ? ESM::VT_Long : ESM::VT_Short);
            result.setInteger(static_cast<std::int32_t>(std::lround(source.mValue)));
        }
        return result;
    }

    const ESM4::GlobalVariable* findGlobal(const MWWorld::ESMStore& store, std::string_view id)
    {
        for (const ESM4::GlobalVariable& source : store.get<ESM4::GlobalVariable>())
            if (Misc::StringUtils::ciEqual(source.mEditorId, id))
                return &source;
        return nullptr;
    }

    bool addSetting(MWWorld::ESMStore& store, std::string_view id, ESM::Variant value)
    {
        if (store.get<ESM::GameSetting>().search(id) != nullptr)
            return false;
        ESM::GameSetting setting;
        setting.blank();
        setting.mId = ESM::RefId::stringRefId(id);
        setting.mValue = std::move(value);
        store.insertStatic(setting);
        return true;
    }

    bool addGlobal(MWWorld::ESMStore& store, std::string_view id, ESM::Variant value)
    {
        const ESM::RefId refId = ESM::RefId::stringRefId(id);
        if (store.get<ESM::Global>().search(refId) != nullptr)
            return false;
        ESM::Global global;
        global.mId = refId;
        global.mValue = std::move(value);
        global.mRecordFlags = 0;
        store.insertStatic(global);
        return true;
    }

    const ESM4::Npc& findPlayer(const MWWorld::ESMStore& store)
    {
        for (const ESM4::Npc& npc : store.get<ESM4::Npc>())
            if (Misc::StringUtils::ciEqual(npc.mEditorId, "Player"))
                return npc;
        throw std::runtime_error("Oblivion profile requires the native NPC_ record with editor ID 'Player'");
    }

    std::array<unsigned char, ESM::Attribute::Length> attributes(const ESM4::AttributeValues& value)
    {
        return { value.strength, value.intelligence, value.willpower, value.agility, value.speed, value.endurance,
            value.personality, value.luck };
    }

    VFS::Path::Normalized meshPath(const ESM::Path& source)
    {
        if (source.empty())
            return VFS::Path::Normalized("meshes/characters/_male/skeleton.nif");
        const VFS::Path::Normalized& value = source.getNormalized();
        if (value.view().starts_with("meshes/"))
            return value;
        return VFS::Path::Normalized("meshes/" + value.value());
    }
}

namespace MWWorld
{
    OblivionProfileInstallReport OblivionProfileServices::install(ESMStore& store)
    {
        OblivionProfileInstallReport report;

        for (const ESM4::GameSetting& source : store.get<ESM4::GameSetting>())
        {
            if (source.mEditorId.empty())
                continue;
            const std::optional<ESM::Variant> value = convertGameSetting(source.mData);
            if (value && addSetting(store, source.mEditorId, *value))
                ++report.mNativeGameSettings;
        }

        const auto addFloat = [&](std::string_view id, float value) {
            report.mRuntimeContractSettings += addSetting(store, id, ESM::Variant(value));
        };
        const auto addInt = [&](std::string_view id, int value) {
            report.mRuntimeContractSettings += addSetting(store, id, ESM::Variant(value));
        };
        const auto addString = [&](std::string_view id, std::string value) {
            report.mRuntimeContractSettings += addSetting(store, id, ESM::Variant(std::move(value)));
        };

        addFloat("fSwimHeightScale", 0.75f);
        addFloat("fStromWindSpeed", 0.f);
        addFloat("fMinWalkSpeed", 100.f);
        addFloat("fMaxWalkSpeed", 200.f);
        addFloat("fMinFlySpeed", 100.f);
        addFloat("fMaxFlySpeed", 200.f);
        addFloat("fSneakSpeedMultiplier", 0.75f);
        addFloat("fEncumbranceStrMult", 5.f);
        addFloat("fJumpMoveBase", 0.5f);
        addFloat("fJumpMoveMult", 0.5f);
        addFloat("fSuffocationDamage", 3.f);
        addFloat("fMessageTimePerChar", 0.02f);
        addFloat("fVanityDelay", 30.f);
        addFloat("fMajorSkillBonus", 0.75f);
        addFloat("fMinorSkillBonus", 1.f);
        addFloat("fMiscSkillBonus", 1.25f);
        addFloat("fSpecialSkillBonus", 0.8f);
        addFloat("fFatigueSneakBase", 1.5f);
        addFloat("fFatigueSneakMult", 1.5f);
        addFloat("fFatigueSwimRunBase", 7.f);
        addFloat("fFatigueSwimRunMult", 0.f);
        addFloat("fFatigueSwimWalkBase", 2.5f);
        addFloat("fFatigueSwimWalkMult", 0.f);
        // Shared OpenMW audio code expresses record attenuation in the same
        // distance scale as TES3. Oblivion keeps these constants in the game
        // executable instead of GMST records, so install the canonical values
        // explicitly for native SOUN/SNDR and voice playback.
        addFloat("fAudioDefaultMinDistance", 5.f);
        addFloat("fAudioDefaultMaxDistance", 40.f);
        addFloat("fAudioVoiceDefaultMinDistance", 10.f);
        addFloat("fAudioVoiceDefaultMaxDistance", 60.f);
        addFloat("fAudioMinDistanceMult", 20.f);
        addFloat("fAudioMaxDistanceMult", 50.f);
        addInt("iMaxActivateDist", 192);
        // Legacy UI and focus services still consume these two common runtime contracts.
        // Oblivion does not ship the identically named Morrowind settings.
        addInt("iMaxInfoDist", 192);
        addInt("iLevelUpTotal", 10);
        addInt("iMonthsToRespawn", 1);
        addString("sDefaultCellname", "Wilderness");
        addString("FontColor_color_header", "223,201,159");
        addString("FontColor_color_normal", "255,255,255");
        addString("fontcolor_color_normal_over", "255,255,255");
        addString("fontcolor_color_normal_pressed", "204,204,204");

        for (std::string_view id : sRuntimeSettingIds)
        {
            if (id.front() == 's')
                addString(id, makeLabel(id));
            else if (id.front() == 'f')
                addFloat(id, 0.f);
            else
                addInt(id, 0);
        }

        for (const ESM4::GlobalVariable& source : store.get<ESM4::GlobalVariable>())
        {
            if (!source.mEditorId.empty() && addGlobal(store, source.mEditorId, convertGlobal(source)))
                ++report.mNativeGlobals;
        }
        const auto addGlobalAlias = [&](GlobalVariableName runtimeId, std::string_view nativeId, ESM::Variant fallback) {
            const ESM4::GlobalVariable* source = findGlobal(store, nativeId);
            addGlobal(store, runtimeId.getValue(), source != nullptr ? convertGlobal(*source) : std::move(fallback));
        };
        addGlobalAlias(Globals::sDaysPassed, "GameDaysPassed", ESM::Variant(1));
        addGlobalAlias(Globals::sGameHour, "GameHour", ESM::Variant(0.f));
        addGlobalAlias(Globals::sTimeScale, "TimeScale", ESM::Variant(30.f));
        addGlobalAlias(Globals::sDay, "GameDay", ESM::Variant(1));
        addGlobalAlias(Globals::sMonth, "GameMonth", ESM::Variant(0));
        addGlobalAlias(Globals::sYear, "GameYear", ESM::Variant(433));
        addGlobal(store, Globals::sCharGenState.getValue(), ESM::Variant(-1));
        addGlobal(store, Globals::sWerewolfClawMult.getValue(), ESM::Variant(0.f));
        addGlobal(store, Globals::sPCKnownWerewolf.getValue(), ESM::Variant(0));
        addGlobal(store, Globals::sPCRace.getValue(), ESM::Variant(0));
        addGlobal(store, Globals::sPCHasCrimeGold.getValue(), ESM::Variant(0));
        addGlobal(store, Globals::sCrimeGoldDiscount.getValue(), ESM::Variant(0));
        addGlobal(store, Globals::sCrimeGoldTurnIn.getValue(), ESM::Variant(0));
        addGlobal(store, Globals::sPCHasTurnIn.getValue(), ESM::Variant(0));

        if (store.get<ESM::MagicEffect>().search(ESM::MagicEffect::Paralyze) == nullptr)
        {
            ESM::MagicEffect paralyze;
            paralyze.blank();
            paralyze.mId = ESM::MagicEffect::Paralyze;
            paralyze.mName = "Paralyze";
            store.insertStatic(paralyze);
        }

        for (int i = 0; i < ESM::Skill::Length; ++i)
        {
            const ESM::RefId id = ESM::Skill::indexToRefId(i);
            if (store.get<ESM::Skill>().search(id) == nullptr)
            {
                ESM::Skill skill;
                skill.blank();
                skill.mId = *id.getIf<ESM::SkillId>();
                skill.mData.mAttribute = ESM::Attribute::indexToRefId(i % ESM::Attribute::Length);
                skill.mData.mSpecialization = i % 3;
                store.insertStatic(skill);
            }
        }

        const ESM4::Npc& nativePlayer = findPlayer(store);
        report.mPlayerSource = nativePlayer.mEditorId + "@" + nativePlayer.mId.toString();
        const ESM4::Race* nativeRace = store.get<ESM4::Race>().search(ESM::RefId(nativePlayer.mRace));
        const ESM4::Class* nativeClass = store.get<ESM4::Class>().search(ESM::RefId(nativePlayer.mClass));

        const ESM::RefId raceId = ESM::RefId::stringRefId("OpenMWOblivionPlayerRace");
        ESM::Race race;
        race.blank();
        race.mId = raceId;
        race.mName = nativeRace != nullptr && !nativeRace->mFullName.empty() ? nativeRace->mFullName : "Imperial";
        race.mData.mFlags = ESM::Race::Playable;
        if (nativeRace != nullptr
            && (Misc::StringUtils::ciEqual(nativeRace->mEditorId, "Argonian")
                || Misc::StringUtils::ciEqual(nativeRace->mEditorId, "Khajiit")))
            race.mData.mFlags |= ESM::Race::Beast;
        if (nativeRace != nullptr)
        {
            race.mData.mMaleHeight = nativeRace->mHeightMale;
            race.mData.mFemaleHeight = nativeRace->mHeightFemale;
            race.mData.mMaleWeight = nativeRace->mWeightMale;
            race.mData.mFemaleWeight = nativeRace->mWeightFemale;
        }
        const std::array<unsigned char, ESM::Attribute::Length> maleRaceAttributes
            = nativeRace != nullptr ? attributes(nativeRace->mAttribMale)
                                    : std::array<unsigned char, ESM::Attribute::Length>{ 40, 40, 40, 40, 40, 40, 40, 40 };
        const std::array<unsigned char, ESM::Attribute::Length> femaleRaceAttributes
            = nativeRace != nullptr ? attributes(nativeRace->mAttribFemale) : maleRaceAttributes;
        for (int i = 0; i < ESM::Attribute::Length; ++i)
        {
            race.mData.setAttribute(ESM::Attribute::indexToRefId(i), true, maleRaceAttributes[i]);
            race.mData.setAttribute(ESM::Attribute::indexToRefId(i), false, femaleRaceAttributes[i]);
        }
        store.insertStatic(race);
        report.mRaceSource = nativeRace != nullptr ? nativeRace->mEditorId + "@" + nativeRace->mId.toString() : "default";

        const ESM::RefId classId = ESM::RefId::stringRefId("OpenMWOblivionPlayerClass");
        ESM::Class characterClass;
        characterClass.blank();
        characterClass.mId = classId;
        characterClass.mName
            = nativeClass != nullptr && !nativeClass->mFullName.empty() ? nativeClass->mFullName : "Adventurer";
        characterClass.mData.mAttribute = { ESM::Attribute::Strength, ESM::Attribute::Endurance };
        characterClass.mData.mSpecialization = ESM::Class::Combat;
        characterClass.mData.mIsPlayable = 1;
        for (std::size_t i = 0; i < characterClass.mData.mSkills.size(); ++i)
        {
            characterClass.mData.mSkills[i][0] = ESM::Skill::indexToRefId(i);
            characterClass.mData.mSkills[i][1]
                = ESM::Skill::indexToRefId(i + characterClass.mData.mSkills.size());
        }
        store.insertStatic(characterClass);
        report.mClassSource
            = nativeClass != nullptr ? nativeClass->mEditorId + "@" + nativeClass->mId.toString() : "default";

        ESM::NPC player;
        player.blank();
        player.mId = ESM::RefId::stringRefId("Player");
        player.mName = nativePlayer.mFullName.empty() ? "Prisoner" : nativePlayer.mFullName;
        player.mRace = raceId;
        player.mClass = classId;
        const VFS::Path::Normalized skeleton = meshPath(nativePlayer.mModel);
        // ESM3 NPC model fields are relative to the meshes VFS root. Keeping
        // the prefix here would make NpcAnimation's normal correction produce
        // "meshes/meshes/..." when it recognizes this as a custom skeleton.
        player.mModel = skeleton.value().substr(std::string_view("meshes/").size());
        player.mFlags = ESM::NPC::Base;
        player.setIsMale((nativePlayer.mBaseConfig.tes4.flags & ESM4::Npc::TES4_Female) == 0);
        player.mNpdt.mLevel = std::max<std::int16_t>(1, nativePlayer.mBaseConfig.tes4.levelOrOffset);
        player.mNpdt.mHealth = static_cast<std::uint16_t>(std::clamp<std::uint32_t>(nativePlayer.mData.health, 1, 65535));
        player.mNpdt.mFatigue = std::max<std::uint16_t>(1, nativePlayer.mBaseConfig.tes4.fatigue);
        const auto playerAttributes = attributes(nativePlayer.mData.attribs);
        for (int i = 0; i < ESM::Attribute::Length; ++i)
            player.mNpdt.mAttributes[ESM::Attribute::indexToRefId(i)]
                = playerAttributes[i] != 0 ? playerAttributes[i] : maleRaceAttributes[i];
        player.mNpdt.mMana = std::max<std::uint16_t>(
            1, static_cast<std::uint16_t>(player.mNpdt.mAttributes[ESM::Attribute::Intelligence] * 2));
        const ESM4::Npc::SkillValues& skills = nativePlayer.mData.skills;
        const std::array nativeSkills{
            std::pair{ ESM::Skill::Armorer, skills.armorer },
            std::pair{ ESM::Skill::Athletics, skills.athletics },
            std::pair{ ESM::Skill::LongBlade, skills.blade },
            std::pair{ ESM::Skill::Block, skills.block },
            std::pair{ ESM::Skill::BluntWeapon, skills.blunt },
            std::pair{ ESM::Skill::HandToHand, skills.handToHand },
            std::pair{ ESM::Skill::HeavyArmor, skills.heavyArmor },
            std::pair{ ESM::Skill::Alchemy, skills.alchemy },
            std::pair{ ESM::Skill::Alteration, skills.alteration },
            std::pair{ ESM::Skill::Conjuration, skills.conjuration },
            std::pair{ ESM::Skill::Destruction, skills.destruction },
            std::pair{ ESM::Skill::Illusion, skills.illusion },
            std::pair{ ESM::Skill::Mysticism, skills.mysticism },
            std::pair{ ESM::Skill::Restoration, skills.restoration },
            std::pair{ ESM::Skill::Acrobatics, skills.acrobatics },
            std::pair{ ESM::Skill::LightArmor, skills.lightArmor },
            std::pair{ ESM::Skill::Marksman, skills.marksman },
            std::pair{ ESM::Skill::Mercantile, skills.mercantile },
            std::pair{ ESM::Skill::Security, skills.security },
            std::pair{ ESM::Skill::Sneak, skills.sneak },
            std::pair{ ESM::Skill::Speechcraft, skills.speechcraft },
        };
        for (int i = 0; i < ESM::Skill::Length; ++i)
            player.mNpdt.mSkills[ESM::Skill::indexToRefId(i)] = 5;
        for (const auto& [id, value] : nativeSkills)
            player.mNpdt.mSkills[ESM::RefId(id)] = value != 0 ? value : 5;
        store.insertStatic(player);

        const VFS::Path::Normalized firstPersonSkeleton("meshes/characters/_1stperson/skeleton.nif");
        const VFS::Path::Normalized beastSkeleton("meshes/characters/_male/skeletonbeast.nif");
        const VFS::Path::Normalized idle("meshes/characters/_male/idle.kf");
        const VFS::Path::Normalized firstPersonIdle("meshes/characters/_1stperson/idle.kf");
        Settings::models().mXbaseanim.set(skeleton);
        Settings::models().mBaseanim.set(skeleton);
        Settings::models().mXbaseanim1st.set(firstPersonSkeleton);
        Settings::models().mBaseanimkna.set(beastSkeleton);
        Settings::models().mBaseanimkna1st.set(firstPersonSkeleton);
        Settings::models().mXbaseanimfemale.set(skeleton);
        Settings::models().mBaseanimfemale.set(skeleton);
        Settings::models().mBaseanimfemale1st.set(firstPersonSkeleton);
        Settings::models().mXbaseanimkf.set(idle);
        Settings::models().mXbaseanim1stkf.set(firstPersonIdle);
        Settings::models().mXbaseanimfemalekf.set(idle);

        // Oblivion ships a complete native sky set under meshes/sky. Select it before the
        // renderer creates any sky nodes so no Morrowind fallback mesh is ever requested.
        Settings::models().mSkyatmosphere.set(VFS::Path::Normalized("meshes/sky/atmosphere.nif"));
        Settings::models().mSkyclouds.set(VFS::Path::Normalized("meshes/sky/clouds.nif"));
        Settings::models().mSkynight01.set(VFS::Path::Normalized("meshes/sky/stars.nif"));
        Settings::models().mSkynight02.set(VFS::Path::Normalized("meshes/sky/stars_oblivion.nif"));
        Settings::models().mWeathersnow.set(VFS::Path::Normalized("meshes/sky/snow.nif"));
        Settings::water().mShader.set(true);

        Log(Debug::Info) << "Installed Oblivion profile services: " << report.mNativeGameSettings
                         << " native GMSTs, " << report.mRuntimeContractSettings << " reviewed runtime settings, "
                         << report.mNativeGlobals << " native globals, player " << report.mPlayerSource << ", race "
                         << report.mRaceSource << ", class " << report.mClassSource;
        return report;
    }
}
