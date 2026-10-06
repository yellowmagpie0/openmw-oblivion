#include "oblivionprofileservices.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>

#include <components/debug/debuglog.hpp>
#include <components/esm/records.hpp>
#include <components/esm3/variant.hpp>
#include <components/esm3/inventorystate.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadclot.hpp>
#include <components/esm4/playermechanics.hpp>
#include <components/esm4/loadappa.hpp>
#include <components/esm4/loadligh.hpp>
#include <components/esm4/loadsgst.hpp>
#include <components/esm4/loadslgm.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/settings/values.hpp>

#include "esmstore.hpp"
#include "class.hpp"
#include "containerstore.hpp"
#include "globals.hpp"
#include "inventorystore.hpp"
#include "oblivioninventoryidentity.hpp"

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
        std::string_view("fSneakUseDelay"),
        std::string_view("fSneakUseDist"),
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

    void setFloatSetting(MWWorld::ESMStore& store, std::string_view id, float value)
    {
        ESM::GameSetting setting;
        setting.blank();
        setting.mId = ESM::RefId::stringRefId(id);
        setting.mValue = ESM::Variant(value);
        store.getWritable<ESM::GameSetting>().insertStatic(setting);
    }

    std::optional<float> nativeFloatSetting(const MWWorld::ESMStore& store, std::string_view id)
    {
        for (const ESM4::GameSetting& setting : store.get<ESM4::GameSetting>())
        {
            if (!Misc::StringUtils::ciEqual(setting.mEditorId, id))
                continue;
            return std::visit(
                [](const auto& value) -> std::optional<float> {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_arithmetic_v<T>)
                        return static_cast<float>(value);
                    else
                        return std::nullopt;
                },
                setting.mData);
        }
        return std::nullopt;
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

    ESM::RefId skillId(std::uint32_t nativeSkill)
    {
        static const std::array ids{ ESM::Skill::Armorer, ESM::Skill::Athletics, ESM::Skill::LongBlade,
            ESM::Skill::Block, ESM::Skill::BluntWeapon, ESM::Skill::HandToHand, ESM::Skill::HeavyArmor,
            ESM::Skill::Alchemy, ESM::Skill::Alteration, ESM::Skill::Conjuration, ESM::Skill::Destruction,
            ESM::Skill::Illusion, ESM::Skill::Mysticism, ESM::Skill::Restoration, ESM::Skill::Acrobatics,
            ESM::Skill::LightArmor, ESM::Skill::Marksman, ESM::Skill::Mercantile, ESM::Skill::Security,
            ESM::Skill::Sneak, ESM::Skill::Speechcraft };
        const int index = ESM4::playerSkillIndex(nativeSkill);
        return index >= 0 ? ESM::RefId(ids[index]) : ESM::RefId{};
    }

    ESM::RefId attributeId(std::uint32_t index)
    {
        return index < ESM::Attribute::Length ? ESM::Attribute::indexToRefId(index) : ESM::RefId{};
    }

    template <class T>
    T clampInteger(std::uint64_t value)
    {
        return static_cast<T>(std::min<std::uint64_t>(value, std::numeric_limits<T>::max()));
    }

    std::string inventoryModel(const ESM::Path& source)
    {
        if (source.empty())
            return {};
        const auto& value = source.getNormalized();
        if (value.view().starts_with("meshes/"))
            return value.value().substr(std::string_view("meshes/").size());
        return value.value();
    }

    ESM::RefId bookSkillId(std::int8_t nativeSkill)
    {
        static const std::array ids{ ESM::Skill::Armorer, ESM::Skill::Athletics, ESM::Skill::LongBlade,
            ESM::Skill::Block, ESM::Skill::BluntWeapon, ESM::Skill::HandToHand, ESM::Skill::HeavyArmor,
            ESM::Skill::Alchemy, ESM::Skill::Alteration, ESM::Skill::Conjuration, ESM::Skill::Destruction,
            ESM::Skill::Illusion, ESM::Skill::Mysticism, ESM::Skill::Restoration, ESM::Skill::Acrobatics,
            ESM::Skill::LightArmor, ESM::Skill::Marksman, ESM::Skill::Mercantile, ESM::Skill::Security,
            ESM::Skill::Sneak, ESM::Skill::Speechcraft };
        return nativeSkill >= 0 && static_cast<std::size_t>(nativeSkill) < ids.size()
            ? ESM::RefId(ids[nativeSkill])
            : ESM::RefId{};
    }

    std::uint16_t enchantmentCapacity(const ESM::FormId& enchantment, std::uint16_t points)
    {
        return enchantment.isZeroOrUnset() ? 0 : points;
    }

    ESM::Weapon projectAmmunition(const ESM4::Ammunition& source)
    {
        ESM::Weapon result;
        result.blank();
        result.mId = ESM::RefId(source.mId);
        result.mName = source.mFullName;
        result.mModel = inventoryModel(source.mModel);
        result.mIcon = source.mIcon;
        result.mData.mType = ESM::Weapon::Arrow;
        result.mData.mWeight = source.mData.mWeight;
        result.mData.mValue = clampInteger<std::int32_t>(source.mData.mValue);
        const auto damage = static_cast<unsigned char>(std::clamp(std::lround(source.mData.mDamage), 0l, 255l));
        result.mData.mChop = result.mData.mSlash = result.mData.mThrust = { damage, damage };
        result.mData.mSpeed = source.mData.mSpeed;
        return result;
    }

    ESM::Apparatus projectApparatus(const ESM4::Apparatus& source)
    {
        ESM::Apparatus result;
        result.blank();
        result.mId = ESM::RefId(source.mId);
        result.mName = source.mFullName;
        result.mModel = inventoryModel(source.mModel);
        result.mIcon = source.mIcon;
        result.mData.mType = std::min<int>(source.mData.type, ESM::Apparatus::Retort);
        result.mData.mQuality = source.mData.quality;
        result.mData.mWeight = source.mData.weight;
        result.mData.mValue = clampInteger<std::int32_t>(source.mData.value);
        return result;
    }

    int armorType(std::uint32_t slots)
    {
        if (slots & ESM4::Armor::TES4_Shield) return ESM::Armor::Shield;
        if (slots & ESM4::Armor::TES4_Head) return ESM::Armor::Helmet;
        if (slots & ESM4::Armor::TES4_UpperBody) return ESM::Armor::Cuirass;
        if (slots & ESM4::Armor::TES4_LowerBody) return ESM::Armor::Greaves;
        if (slots & ESM4::Armor::TES4_Feet) return ESM::Armor::Boots;
        if (slots & ESM4::Armor::TES4_Hands) return ESM::Armor::RGauntlet;
        return ESM::Armor::Cuirass;
    }

    ESM::Armor projectArmor(const ESM4::Armor& source)
    {
        ESM::Armor result;
        result.blank();
        result.mId = ESM::RefId(source.mId);
        result.mName = source.mFullName;
        result.mModel = inventoryModel(source.mModelMaleWorld.empty() ? source.mModelMale : source.mModelMaleWorld);
        result.mIcon = source.mIconMale;
        result.mData.mType = armorType(source.mArmorFlags);
        result.mData.mWeight = source.mData.weight;
        result.mData.mValue = clampInteger<std::int32_t>(source.mData.value);
        result.mData.mHealth = clampInteger<std::int32_t>(source.mData.health);
        result.mData.mArmor = source.mData.armor;
        result.mData.mEnchant = enchantmentCapacity(source.mEnchantment, source.mEnchantmentPoints);
        return result;
    }

    int clothingType(std::uint32_t slots)
    {
        if (slots & (ESM4::Armor::TES4_RightRing | ESM4::Armor::TES4_LeftRing)) return ESM::Clothing::Ring;
        if (slots & ESM4::Armor::TES4_Amulet) return ESM::Clothing::Amulet;
        if ((slots & (ESM4::Armor::TES4_UpperBody | ESM4::Armor::TES4_LowerBody))
            == (ESM4::Armor::TES4_UpperBody | ESM4::Armor::TES4_LowerBody))
            return ESM::Clothing::Robe;
        if (slots & ESM4::Armor::TES4_UpperBody) return ESM::Clothing::Shirt;
        if (slots & ESM4::Armor::TES4_LowerBody) return ESM::Clothing::Pants;
        if (slots & ESM4::Armor::TES4_Feet) return ESM::Clothing::Shoes;
        if (slots & ESM4::Armor::TES4_Hands) return ESM::Clothing::RGlove;
        return ESM::Clothing::Robe;
    }

    ESM::Clothing projectClothing(const ESM4::Clothing& source)
    {
        ESM::Clothing result;
        result.blank();
        result.mId = ESM::RefId(source.mId);
        result.mName = source.mFullName;
        result.mModel = inventoryModel(source.mModelMaleWorld.empty() ? source.mModelMale : source.mModelMaleWorld);
        result.mIcon = source.mIconMale;
        result.mData.mType = clothingType(source.mClothingFlags);
        result.mData.mWeight = source.mData.weight;
        result.mData.mValue = clampInteger<std::uint16_t>(source.mData.value);
        result.mData.mEnchant = enchantmentCapacity(source.mEnchantment, source.mEnchantmentPoints);
        return result;
    }

    ESM::Book projectBook(const ESM4::Book& source)
    {
        ESM::Book result;
        result.blank();
        result.mId = ESM::RefId(source.mId);
        result.mName = source.mFullName;
        result.mModel = inventoryModel(source.mModel);
        result.mIcon = source.mIcon;
        result.mText = source.mText;
        result.mData.mSkillId = bookSkillId(source.mData.bookSkill);
        result.mData.mWeight = source.mData.weight;
        result.mData.mValue = clampInteger<std::int32_t>(source.mData.value);
        result.mData.mIsScroll = (source.mData.flags & ESM4::Book::Flag_Scroll) != 0;
        result.mData.mEnchant = enchantmentCapacity(source.mEnchantment, source.mEnchantmentPoints);
        return result;
    }

    ESM::Ingredient projectIngredient(const ESM4::Ingredient& source)
    {
        ESM::Ingredient result;
        result.blank();
        result.mId = ESM::RefId(source.mId);
        result.mName = source.mFullName;
        result.mModel = inventoryModel(source.mModel);
        result.mIcon = source.mIcon;
        result.mData.mWeight = source.mData.weight;
        result.mData.mValue = clampInteger<std::int32_t>(source.mData.value);
        return result;
    }

    template <class Source>
    ESM::Miscellaneous projectMisc(const Source& source, bool key)
    {
        ESM::Miscellaneous result;
        result.blank();
        result.mId = ESM::RefId(source.mId);
        result.mName = source.mFullName;
        result.mModel = inventoryModel(source.mModel);
        result.mIcon = source.mIcon;
        result.mData.mWeight = source.mData.weight;
        result.mData.mValue = clampInteger<std::int32_t>(source.mData.value);
        result.mData.mFlags = key ? ESM::Miscellaneous::Key : 0;
        return result;
    }

    ESM::Lockpick projectLockpick(const ESM4::MiscItem& source)
    {
        ESM::Lockpick result;
        result.blank();
        result.mId = ESM::RefId(source.mId);
        result.mName = source.mFullName;
        result.mModel = inventoryModel(source.mModel);
        result.mIcon = source.mIcon;
        result.mData.mWeight = source.mData.weight;
        result.mData.mValue = clampInteger<std::int32_t>(source.mData.value);
        result.mData.mQuality = 1.f;
        result.mData.mUses = 1;
        return result;
    }

    ESM::Repair projectRepair(const ESM4::MiscItem& source)
    {
        ESM::Repair result;
        result.blank();
        result.mId = ESM::RefId(source.mId);
        result.mName = source.mFullName;
        result.mModel = inventoryModel(source.mModel);
        result.mIcon = source.mIcon;
        result.mData.mWeight = source.mData.weight;
        result.mData.mValue = clampInteger<std::int32_t>(source.mData.value);
        result.mData.mQuality = 1.f;
        result.mData.mUses = 1;
        return result;
    }

    ESM::Potion projectPotion(const ESM4::Potion& source)
    {
        ESM::Potion result;
        result.blank();
        result.mId = ESM::RefId(source.mId);
        result.mName = source.mFullName;
        result.mModel = inventoryModel(source.mModel);
        result.mIcon = source.mIcon;
        result.mData.mWeight = source.mData.weight;
        result.mData.mValue = std::max(0, source.mItem.value);
        return result;
    }

    ESM::Light projectLight(const ESM4::Light& source)
    {
        ESM::Light result;
        result.blank();
        result.mId = ESM::RefId(source.mId);
        result.mName = source.mFullName;
        result.mModel = inventoryModel(source.mModel);
        result.mIcon = source.mIcon;
        result.mSound = ESM::RefId(source.mSound);
        result.mData.mWeight = source.mData.weight;
        result.mData.mValue = clampInteger<std::int32_t>(source.mData.value);
        result.mData.mTime = source.mData.time;
        result.mData.mRadius = clampInteger<std::int32_t>(source.mData.radius);
        result.mData.mColor = source.mData.colour;
        result.mData.mFlags = source.mData.flags;
        return result;
    }

    int weaponType(std::uint32_t type)
    {
        static constexpr std::array types{ ESM::Weapon::LongBladeOneHand, ESM::Weapon::LongBladeTwoHand,
            ESM::Weapon::BluntOneHand, ESM::Weapon::BluntTwoWide, ESM::Weapon::BluntTwoWide,
            ESM::Weapon::MarksmanBow };
        return type < types.size() ? types[type] : ESM::Weapon::LongBladeOneHand;
    }

    ESM::Weapon projectWeapon(const ESM4::Weapon& source)
    {
        ESM::Weapon result;
        result.blank();
        result.mId = ESM::RefId(source.mId);
        result.mName = source.mFullName;
        result.mModel = inventoryModel(source.mModel);
        result.mIcon = source.mIcon;
        result.mData.mType = weaponType(source.mData.type);
        result.mData.mWeight = source.mData.weight;
        result.mData.mValue = clampInteger<std::int32_t>(source.mData.value);
        result.mData.mHealth = clampInteger<std::uint16_t>(source.mData.health);
        result.mData.mSpeed = source.mData.speed;
        result.mData.mReach = source.mData.reach;
        result.mData.mEnchant = enchantmentCapacity(source.mEnchantment, source.mEnchantmentPoints);
        const auto damage = clampInteger<unsigned char>(source.mData.damage);
        result.mData.mChop = result.mData.mSlash = result.mData.mThrust = { damage, damage };
        return result;
    }

    ESM::Race projectRace(const ESM4::Race& source)
    {
        ESM::Race race;
        race.blank();
        race.mId = ESM::RefId(source.mId);
        race.mName = source.mFullName;
        race.mDescription = source.mDesc;
        if ((source.mRaceFlags & 1) != 0)
            race.mData.mFlags |= ESM::Race::Playable;
        if (Misc::StringUtils::ciEqual(source.mEditorId, "Argonian")
            || Misc::StringUtils::ciEqual(source.mEditorId, "Khajiit"))
            race.mData.mFlags |= ESM::Race::Beast;
        race.mData.mMaleHeight = source.mHeightMale;
        race.mData.mFemaleHeight = source.mHeightFemale;
        race.mData.mMaleWeight = source.mWeightMale;
        race.mData.mFemaleWeight = source.mWeightFemale;
        const auto male = attributes(source.mAttribMale);
        const auto female = attributes(source.mAttribFemale);
        for (int i = 0; i < ESM::Attribute::Length; ++i)
        {
            race.mData.setAttribute(ESM::Attribute::indexToRefId(i), true, male[i]);
            race.mData.setAttribute(ESM::Attribute::indexToRefId(i), false, female[i]);
        }
        std::size_t bonusIndex = 0;
        for (const auto& [nativeSkill, bonus] : source.mSkillBonus)
        {
            const ESM::RefId id = skillId(nativeSkill);
            if (!id.empty() && bonusIndex < race.mData.mBonus.size())
                race.mData.mBonus[bonusIndex++] = { id, bonus };
        }
        for (const ESM::FormId power : source.mBonusSpells)
            race.mPowers.mList.emplace_back(power);
        return race;
    }

    ESM::Class projectClass(const ESM4::Class& source)
    {
        ESM::Class result;
        result.blank();
        result.mId = ESM::RefId(source.mId);
        result.mName = source.mFullName;
        result.mDescription = source.mDesc;
        for (std::size_t i = 0; i < result.mData.mAttribute.size(); ++i)
            result.mData.mAttribute[i] = attributeId(source.mData.mFavoredAttributes[i]);
        result.mData.mSpecialization = std::min<std::uint32_t>(source.mData.mSpecialization, ESM::Class::Stealth);
        result.mData.mIsPlayable = (source.mData.mFlags & 1) != 0;
        result.mData.mServices = source.mData.mServices;
        for (std::size_t i = 0; i < 5; ++i)
            result.mData.mSkills[i][1] = skillId(source.mData.mMajorSkills[i]);
        result.mData.mSkills[0][0] = skillId(source.mData.mMajorSkills[5]);
        result.mData.mSkills[1][0] = skillId(source.mData.mMajorSkills[6]);
        return result;
    }

    ESM::BirthSign projectBirthSign(const ESM4::BirthSign& source)
    {
        ESM::BirthSign result;
        result.blank();
        result.mId = ESM::RefId(source.mId);
        result.mName = source.mFullName;
        result.mDescription = source.mDescription;
        result.mTexture = source.mIcon;
        for (const ESM::FormId spell : source.mSpells)
            result.mPowers.mList.emplace_back(spell);
        return result;
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
    ESM::RefId OblivionProfileServices::resolveOwnershipGlobal(const ESMStore& store,
        const ESM::FormKeyResolver& resolver, const ESM::FormKey& key)
    {
        if (ESM::FormKey::deserialize(key.serialize()) != key)
            throw std::invalid_argument("Native ownership global is not canonical");
        if (key.isNull()) return {};
        const auto id = resolver.toFormId(key);
        if (!key.isContent() || !id || !store.get<ESM4::GlobalVariable>().search(ESM::RefId(*id)))
            throw std::invalid_argument("Native ownership global is not a winning GLOB: " + key.serialize());
        return ESM::RefId(*id);
    }

    ESM::FormKey OblivionProfileServices::captureOwnershipGlobal(const ESMStore& store,
        const ESM::FormKeyResolver& resolver, const CellRef& reference)
    {
        const auto value = reference.getNativeOwnershipGlobal();
        if (value.empty()) return {};
        const auto* id = value.getIf<ESM::FormId>();
        if (!id) throw std::invalid_argument("Native ownership global is not a FormId");
        const auto key = resolver.toFormKey(*id);
        resolveOwnershipGlobal(store, resolver, key);
        return key;
    }

    std::vector<PreparedOblivionInventoryItem> OblivionProfileServices::prepareActorInventory(
        const ESMStore& store, const ESM::FormKeyResolver& resolver,
        const std::vector<ESM4::RuntimeInventoryItem>& items, const ESMStore* incoming)
    {
        std::vector<PreparedOblivionInventoryItem> result;
        result.reserve(items.size());
        for (const auto& item : items)
        {
            if (!std::isfinite(item.mCondition) || (item.mCondition < 0 && item.mCondition != -1)
                || item.mCondition > std::numeric_limits<float>::max())
                throw std::invalid_argument("Invalid native inventory condition");
            const auto generatedId = OblivionInventory::sharedId(item.mBase);
            const auto itemId = resolver.toFormId(item.mBase);
            if (!generatedId && (!itemId || !itemDefinition(store, ESM::RefId(*itemId))))
                throw std::runtime_error("TES4 runtime-state actor item cannot be resolved: "
                    + item.mBase.serialize());
            ManualRef source(store, generatedId ? *generatedId : sharedItemId(store, ESM::RefId(*itemId)),
                item.mCount, incoming);
            const Ptr ptr = source.getPtr();
            // Generated classes, actors, leveled lists and other definitions
            // are not actual inventory stacks, even when their ID resolves.
            if (generatedId)
                ContainerStore::getType(ptr);
            if (item.mCondition >= 0)
                ptr.getCellRef().setNativeItemCondition(static_cast<float>(item.mCondition));
            if (item.mCharge >= 0.f)
                ptr.getCellRef().setEnchantmentCharge(item.mCharge);
            if (item.mRemainingUsageTime >= 0.f)
                ptr.getClass().setRemainingUsageTime(ptr, item.mRemainingUsageTime);
            if (!item.mOwner.isNull())
            {
                const auto ownerId = resolver.toFormId(item.mOwner);
                if (!ownerId)
                    throw std::runtime_error("TES4 runtime-state actor item owner cannot be resolved: "
                        + item.mOwner.serialize());
                ptr.getCellRef().setOwner(ESM::RefId(*ownerId));
            }
            ptr.getCellRef().setNativeOwnershipGlobal(resolveOwnershipGlobal(store, resolver, item.mOwnershipGlobal));
            ptr.getCellRef().setNativeOwnershipRank(item.mOwnershipRank);
            std::optional<int> slot;
            if (item.mEquippedSlots != 0)
            {
                const auto equipment = ptr.getClass().getEquipmentSlots(ptr);
                const auto& slots = equipment.first;
                if (generatedId)
                {
                    for (const int candidate : slots)
                        if (OblivionInventory::slotMask(candidate, ptr.getType() == ESM::REC_LIGH)
                            == item.mEquippedSlots)
                        {
                            slot = candidate;
                            break;
                        }
                    if (!slot || (item.mCount != 1 && !equipment.second))
                        throw std::invalid_argument("Generated inventory equipment disagrees with its definition");
                }
                else if (!slots.empty())
                {
                    slot = slots.front();
                    if ((item.mEquippedSlots & ESM4::Armor::TES4_LeftRing) != 0
                        && std::ranges::find(slots, InventoryStore::Slot_LeftRing) != slots.end())
                        slot = InventoryStore::Slot_LeftRing;
                }
            }
            result.push_back({std::move(source), slot});
        }
        return result;
    }

    std::unique_ptr<InventoryStore> OblivionProfileServices::stageActorInventory(
        const std::vector<PreparedOblivionInventoryItem>& items)
    {
        struct StagedInventory final : InventoryStore
        {
            explicit StagedInventory(const std::vector<PreparedOblivionInventoryItem>& items)
            {
                // Establish resolved/modified ownership without loading or
                // registering any references through readState's item path.
                readState({});
                for (const auto& item : items)
                {
                    const Ptr ptr = item.mReference.getPtr();
                    if (ptr.mRef->mWorldModel != nullptr)
                        throw std::invalid_argument("native inventory staging requires detached source items");
                    const auto added = addNewStack(ptr, ptr.getCellRef().getCount(false));
                    if (item.mEquipmentSlot)
                    {
                        ESM::InventoryState equipment;
                        equipment.mEquipmentSlots.emplace(0, *item.mEquipmentSlot);
                        readEquipmentState(added, 0, equipment);
                    }
                }
            }
        };
        return std::make_unique<StagedInventory>(items);
    }

    ESM::RefId OblivionProfileServices::sharedItemId(const ESMStore& store, const ESM::RefId& nativeId)
    {
        if (const ESM4::MiscItem* item = store.get<ESM4::MiscItem>().search(nativeId);
            item != nullptr && Misc::StringUtils::ciEqual(item->mEditorId, "Gold001"))
            return ContainerStore::sGoldId;
        // Runtime FormIds carry a load-order byte, while the projected shared
        // records retain the authoritative local FormId from their TES4
        // definition. Resolve through the native store and return that exact
        // definition ID so ManualRef can construct the projected item.
        const auto definitionId = [&](const auto& nativeStore) -> std::optional<ESM::RefId> {
            if (const auto* item = nativeStore.search(nativeId))
                return ESM::RefId(item->mId);
            return std::nullopt;
        };
#define OPENMW_TES4_SHARED_ITEM_ID(Type) \
        if (const auto id = definitionId(store.get<ESM4::Type>()); id) return *id
        OPENMW_TES4_SHARED_ITEM_ID(Ammunition);
        OPENMW_TES4_SHARED_ITEM_ID(Apparatus);
        OPENMW_TES4_SHARED_ITEM_ID(Armor);
        OPENMW_TES4_SHARED_ITEM_ID(Book);
        OPENMW_TES4_SHARED_ITEM_ID(Clothing);
        OPENMW_TES4_SHARED_ITEM_ID(Ingredient);
        OPENMW_TES4_SHARED_ITEM_ID(Key);
        OPENMW_TES4_SHARED_ITEM_ID(Light);
        OPENMW_TES4_SHARED_ITEM_ID(MiscItem);
        OPENMW_TES4_SHARED_ITEM_ID(Potion);
        OPENMW_TES4_SHARED_ITEM_ID(SigilStone);
        OPENMW_TES4_SHARED_ITEM_ID(SoulGem);
        OPENMW_TES4_SHARED_ITEM_ID(Weapon);
#undef OPENMW_TES4_SHARED_ITEM_ID
        return nativeId;
    }

    ESM::RefId OblivionProfileServices::nativeItemId(const ESMStore& store, const ESM::RefId& sharedId)
    {
        if (sharedId != ContainerStore::sGoldId)
            return sharedId;
        for (const ESM4::MiscItem& item : store.get<ESM4::MiscItem>())
            if (Misc::StringUtils::ciEqual(item.mEditorId, "Gold001"))
                return ESM::RefId(item.mId);
        return sharedId;
    }

    int OblivionProfileServices::sharedItemType(const ESMStore& store, const ESM::RefId& sharedId)
    {
        // ESMStore::find intentionally reports the authoritative TES4 record
        // type for a FormId. ContainerStore, however, persists the projected
        // TES3 facade type. Resolve that facade explicitly when restoring an
        // Oblivion inventory so a native ARMO/WEAP/etc. does not get rejected
        // as an invalid shared inventory record.
        if (store.get<ESM::Potion>().search(sharedId) != nullptr)
            return ESM::REC_ALCH;
        if (store.get<ESM::Apparatus>().search(sharedId) != nullptr)
            return ESM::REC_APPA;
        if (store.get<ESM::Armor>().search(sharedId) != nullptr)
            return ESM::REC_ARMO;
        if (store.get<ESM::Book>().search(sharedId) != nullptr)
            return ESM::REC_BOOK;
        if (store.get<ESM::Clothing>().search(sharedId) != nullptr)
            return ESM::REC_CLOT;
        if (store.get<ESM::Ingredient>().search(sharedId) != nullptr)
            return ESM::REC_INGR;
        if (store.get<ESM::Lockpick>().search(sharedId) != nullptr)
            return ESM::REC_LOCK;
        if (store.get<ESM::Repair>().search(sharedId) != nullptr)
            return ESM::REC_REPA;
        if (store.get<ESM::Light>().search(sharedId) != nullptr)
            return ESM::REC_LIGH;
        if (store.get<ESM::Weapon>().search(sharedId) != nullptr)
            return ESM::REC_WEAP;
        if (store.get<ESM::Miscellaneous>().search(sharedId) != nullptr)
            return ESM::REC_MISC;
        return 0;
    }

    std::optional<ESM4::InventoryItemDefinition> OblivionProfileServices::itemDefinition(
        const ESMStore& store, const ESM::RefId& id)
    {
        ESM4::InventoryItemDefinition result;
        if (const ESM4::Ammunition* ammunition = store.get<ESM4::Ammunition>().search(id))
        {
            result.mType = ESM4::InventoryItemType::Ammunition;
            result.mValue = clampInteger<std::int32_t>(ammunition->mData.mValue);
            result.mWeight = ammunition->mData.mWeight;
            result.mSlots = ESM4::InventorySlotAmmunition;
        }
        else if (const ESM4::Apparatus* apparatus = store.get<ESM4::Apparatus>().search(id))
        {
            result.mType = ESM4::InventoryItemType::Apparatus;
            result.mValue = clampInteger<std::int32_t>(apparatus->mData.value);
            result.mWeight = apparatus->mData.weight;
        }
        else if (const ESM4::Armor* armor = store.get<ESM4::Armor>().search(id))
        {
            result.mType = ESM4::InventoryItemType::Armor;
            result.mValue = clampInteger<std::int32_t>(armor->mData.value);
            result.mWeight = armor->mData.weight;
            result.mMaxCondition
                = armor->mData.health == 0 ? -1 : clampInteger<std::int32_t>(armor->mData.health);
            const std::uint16_t capacity = enchantmentCapacity(armor->mEnchantment, armor->mEnchantmentPoints);
            result.mMaxCharge = capacity == 0 ? -1.f : capacity;
            result.mSlots = armor->mArmorFlags & 0xffffu;
            result.mChooseOneSlot = result.mSlots
                == (ESM4::Armor::TES4_RightRing | ESM4::Armor::TES4_LeftRing);
        }
        else if (const ESM4::Book* book = store.get<ESM4::Book>().search(id))
        {
            result.mType = (book->mData.flags & ESM4::Book::Flag_Scroll) != 0
                ? ESM4::InventoryItemType::Scroll
                : ESM4::InventoryItemType::Book;
            result.mValue = clampInteger<std::int32_t>(book->mData.value);
            result.mWeight = book->mData.weight;
            const std::uint16_t capacity = enchantmentCapacity(book->mEnchantment, book->mEnchantmentPoints);
            result.mMaxCharge = capacity == 0 ? -1.f : capacity;
            result.mConsumable = result.mType == ESM4::InventoryItemType::Scroll;
        }
        else if (const ESM4::Clothing* clothing = store.get<ESM4::Clothing>().search(id))
        {
            result.mType = ESM4::InventoryItemType::Clothing;
            result.mValue = clampInteger<std::int32_t>(clothing->mData.value);
            result.mWeight = clothing->mData.weight;
            const std::uint16_t capacity = enchantmentCapacity(clothing->mEnchantment, clothing->mEnchantmentPoints);
            result.mMaxCharge = capacity == 0 ? -1.f : capacity;
            result.mSlots = clothing->mClothingFlags & 0xffffu;
            result.mChooseOneSlot = result.mSlots
                == (ESM4::Armor::TES4_RightRing | ESM4::Armor::TES4_LeftRing);
        }
        else if (const ESM4::Ingredient* ingredient = store.get<ESM4::Ingredient>().search(id))
        {
            result.mType = ESM4::InventoryItemType::Ingredient;
            result.mValue = clampInteger<std::int32_t>(ingredient->mData.value);
            result.mWeight = ingredient->mData.weight;
            result.mConsumable = true;
        }
        else if (const ESM4::Key* key = store.get<ESM4::Key>().search(id))
        {
            result.mType = ESM4::InventoryItemType::Key;
            result.mValue = clampInteger<std::int32_t>(key->mData.value);
            result.mWeight = key->mData.weight;
        }
        else if (const ESM4::Light* light = store.get<ESM4::Light>().search(id))
        {
            if ((light->mData.flags & ESM4::Light::Carryable) == 0)
                return std::nullopt;
            result.mType = ESM4::InventoryItemType::Light;
            result.mValue = clampInteger<std::int32_t>(light->mData.value);
            result.mWeight = light->mData.weight;
            result.mSlots = ESM4::InventorySlotLight;
            result.mMaxUsageTime = light->mData.time > 0 ? static_cast<float>(light->mData.time) : -1.f;
        }
        else if (const ESM4::MiscItem* miscellaneous = store.get<ESM4::MiscItem>().search(id))
        {
            result.mType = ESM4::InventoryItemType::Miscellaneous;
            result.mValue = clampInteger<std::int32_t>(miscellaneous->mData.value);
            result.mWeight = miscellaneous->mData.weight;
        }
        else if (const ESM4::Potion* potion = store.get<ESM4::Potion>().search(id))
        {
            result.mType = ESM4::InventoryItemType::Potion;
            result.mValue = std::max(0, potion->mItem.value);
            result.mWeight = potion->mData.weight;
            result.mConsumable = true;
            result.mFood = (potion->mItem.flags & 0x00000002u) != 0;
        }
        else if (const ESM4::SigilStone* stone = store.get<ESM4::SigilStone>().search(id))
        {
            result.mType = ESM4::InventoryItemType::SigilStone;
            result.mValue = clampInteger<std::int32_t>(stone->mData.value);
            result.mWeight = stone->mData.weight;
            result.mConsumable = true;
        }
        else if (const ESM4::SoulGem* gem = store.get<ESM4::SoulGem>().search(id))
        {
            result.mType = ESM4::InventoryItemType::SoulGem;
            result.mValue = clampInteger<std::int32_t>(gem->mData.value);
            result.mWeight = gem->mData.weight;
            result.mConsumable = gem->mSoul != 0;
        }
        else if (const ESM4::Weapon* weapon = store.get<ESM4::Weapon>().search(id))
        {
            result.mType = ESM4::InventoryItemType::Weapon;
            result.mValue = clampInteger<std::int32_t>(weapon->mData.value);
            result.mWeight = weapon->mData.weight;
            result.mMaxCondition
                = weapon->mData.health == 0 ? -1 : clampInteger<std::int32_t>(weapon->mData.health);
            const std::uint16_t capacity = enchantmentCapacity(weapon->mEnchantment, weapon->mEnchantmentPoints);
            result.mMaxCharge = capacity == 0 ? -1.f : capacity;
            result.mSlots = ESM4::InventorySlotWeapon;
        }
        else
            return std::nullopt;
        return result;
    }

    void OblivionProfileServices::equipNativeApparel(InventoryStore& inventory, const ESMStore& store)
    {
        struct Candidate
        {
            ESM::RefId mSharedId;
            std::vector<int> mSharedSlots;
            std::uint32_t mNativeSlots = 0;
            std::int32_t mValue = 0;
            bool mChooseOneSlot = false;
        };

        std::vector<Candidate> candidates;
        for (ContainerStoreIterator iterator = inventory.begin(); iterator != inventory.end(); ++iterator)
        {
            const Ptr item = *iterator;
            if (item.isEmpty() || item.getCellRef().getCount() != 1)
                continue;
            const ESM::RefId nativeId = nativeItemId(store, item.getCellRef().getRefId());
            const auto definition = itemDefinition(store, nativeId);
            if (!definition || (definition->mType != ESM4::InventoryItemType::Armor
                                   && definition->mType != ESM4::InventoryItemType::Clothing)
                || definition->mSlots == 0)
                continue;
            const std::vector<int> sharedSlots = item.getClass().getEquipmentSlots(item).first;
            if (sharedSlots.empty())
                continue;
            candidates.push_back({ item.getCellRef().getRefId(), sharedSlots, definition->mSlots,
                definition->mValue, definition->mChooseOneSlot });
        }

        std::sort(candidates.begin(), candidates.end(), [](const Candidate& left, const Candidate& right) {
            if (left.mValue != right.mValue)
                return left.mValue > right.mValue;
            return left.mSharedId < right.mSharedId;
        });

        std::uint32_t occupied = 0;
        for (const Candidate& candidate : candidates)
        {
            std::uint32_t selectedNativeSlots = candidate.mNativeSlots;
            int selectedSharedSlot = candidate.mSharedSlots.front();
            if (candidate.mChooseOneSlot)
            {
                const std::uint32_t available = candidate.mNativeSlots & ~occupied;
                if (available == 0)
                    continue;
                if ((available & ESM4::Armor::TES4_LeftRing) != 0
                    && std::find(candidate.mSharedSlots.begin(), candidate.mSharedSlots.end(),
                                    InventoryStore::Slot_LeftRing)
                        != candidate.mSharedSlots.end())
                {
                    selectedSharedSlot = InventoryStore::Slot_LeftRing;
                    selectedNativeSlots = ESM4::Armor::TES4_LeftRing;
                }
                else
                {
                    selectedSharedSlot = InventoryStore::Slot_RightRing;
                    selectedNativeSlots = ESM4::Armor::TES4_RightRing;
                }
            }
            else if ((candidate.mNativeSlots & occupied) != 0)
                continue;

            ContainerStoreIterator item = inventory.end();
            for (ContainerStoreIterator iterator = inventory.begin(); iterator != inventory.end(); ++iterator)
            {
                if (iterator->getCellRef().getRefId() == candidate.mSharedId
                    && iterator->getCellRef().getCount() == 1)
                {
                    item = iterator;
                    break;
                }
            }
            if (item == inventory.end())
                continue;
            try
            {
                inventory.equip(selectedSharedSlot, item);
                occupied = (occupied & ~candidate.mNativeSlots) | selectedNativeSlots;
            }
            catch (const std::exception& error)
            {
                Log(Debug::Warning) << "TES4 native apparel could not be equipped for "
                                    << candidate.mSharedId.toDebugString() << ": " << error.what();
            }
        }
    }

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
        // The shared stealth observer update uses this TES3-named contract. Oblivion's equivalent is implemented
        // in the executable rather than exposed as GMSTs; retain the shared one-second sampling cadence and
        // 500-unit interaction radius.
        addFloat("fSneakUseDelay", 1.f);
        addFloat("fSneakUseDist", 500.f);
        // Shared actor head-tracking still consumes these TES3-named
        // settings. Oblivion keeps the equivalent values in its executable;
        // install the reviewed native defaults so a TES4 actor never trips a
        // missing-GMST error in the common actor pass.
        addFloat("fMaxHeadTrackDistance", 400.f);
        addFloat("fInteriorHeadTrackMult", 0.5f);
        // Oblivion keeps these greeting constants in its executable rather than
        // exposing them as TES4 GMST records. Install the reviewed native values
        // for the dialogue-approach bridge.
        addInt("iGreetDistanceMultiplier", 6);
        addFloat("fGreetDistanceReset", 2000.f);
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
        addInt("iMaxActivateDist", 150);
        // Legacy UI and focus services still consume these two common runtime contracts.
        // Oblivion does not ship the identically named Morrowind settings.
        addInt("iMaxInfoDist", 150);
        addInt("iLevelUpTotal", 10);
        addInt("iMonthsToRespawn", 1);
        addString("sDefaultCellname", "Wilderness");
        addString("FontColor_color_header", "223,201,159");
        addString("FontColor_color_normal", "255,255,255");
        addString("fontcolor_color_normal_over", "255,255,255");
        addString("fontcolor_color_normal_pressed", "204,204,204");
        // TES4's actor-value display names are not carried under the TES3 GMST identifiers used by the shared UI.
        // Populate those identifiers with the actual Oblivion names so character creation never exposes compatibility
        // enum names such as "Longblade" or generated "Skill ..." placeholders.
        for (const auto& [id, name] : std::array{
                 std::pair{ "sAttributeStrength", "Strength" },
                 std::pair{ "sAttributeIntelligence", "Intelligence" },
                 std::pair{ "sAttributeWillpower", "Willpower" },
                 std::pair{ "sAttributeAgility", "Agility" },
                 std::pair{ "sAttributeSpeed", "Speed" },
                 std::pair{ "sAttributeEndurance", "Endurance" },
                 std::pair{ "sAttributePersonality", "Personality" },
                 std::pair{ "sAttributeLuck", "Luck" },
                 std::pair{ "sSkillArmorer", "Armorer" },
                 std::pair{ "sSkillAthletics", "Athletics" },
                 std::pair{ "sSkillLongblade", "Blade" },
                 std::pair{ "sSkillBlock", "Block" },
                 std::pair{ "sSkillBluntweapon", "Blunt" },
                 std::pair{ "sSkillHandtohand", "Hand to Hand" },
                 std::pair{ "sSkillHeavyarmor", "Heavy Armor" },
                 std::pair{ "sSkillAlchemy", "Alchemy" },
                 std::pair{ "sSkillAlteration", "Alteration" },
                 std::pair{ "sSkillConjuration", "Conjuration" },
                 std::pair{ "sSkillDestruction", "Destruction" },
                 std::pair{ "sSkillIllusion", "Illusion" },
                 std::pair{ "sSkillMysticism", "Mysticism" },
                 std::pair{ "sSkillRestoration", "Restoration" },
                 std::pair{ "sSkillAcrobatics", "Acrobatics" },
                 std::pair{ "sSkillLightarmor", "Light Armor" },
                 std::pair{ "sSkillMarksman", "Marksman" },
                 std::pair{ "sSkillMercantile", "Mercantile" },
                 std::pair{ "sSkillSecurity", "Security" },
                 std::pair{ "sSkillSneak", "Sneak" },
                 std::pair{ "sSkillSpeechcraft", "Speechcraft" },
                 std::pair{ "sSpecializationCombat", "Combat" },
                 std::pair{ "sSpecializationMagic", "Magic" },
                 std::pair{ "sSpecializationStealth", "Stealth" },
                 std::pair{ "sSkillClassMajor", "Major Skills" },
                 std::pair{ "sSkillClassMinor", "Major Skills 6-7" },
                 std::pair{ "sSkillClassMisc", "Other Skills" },
                 std::pair{ "sChooseClassMenu1", "Specialization" },
                 std::pair{ "sChooseClassMenu2", "Favored Attributes" },
                 std::pair{ "sChooseClassMenu3", "Major Skills" },
                 std::pair{ "sChooseClassMenu4", "Major Skills 6-7" },
                 std::pair{ "sBack", "Back" },
                 std::pair{ "sNext", "Next" },
                 std::pair{ "sDone", "Done" },
                 std::pair{ "sOK", "OK" },
             })
            addString(id, name);

        for (std::string_view id : sRuntimeSettingIds)
        {
            if (id.front() == 's')
                addString(id, makeLabel(id));
            else if (id.front() == 'f')
                addFloat(id, 0.f);
            else
                addInt(id, 0);
        }

        const auto aliasFloat = [&](std::string_view runtimeId, std::string_view nativeId, float fallback) {
            setFloatSetting(store, runtimeId, nativeFloatSetting(store, nativeId).value_or(fallback));
        };
        aliasFloat("fMinWalkSpeed", "fMoveCharWalkMin", 90.f);
        aliasFloat("fMaxWalkSpeed", "fMoveCharWalkMax", 130.f);
        aliasFloat("fBaseRunMultiplier", "fMoveRunMult", 3.f);
        aliasFloat("fAthleticsRunBonus", "fMoveRunAthleticsMult", 0.f);
        aliasFloat("fEncumberedMoveEffect", "fMoveEncumEffect", 0.4f);
        aliasFloat("fEncumbranceStrMult", "fActorStrengthEncumbranceMult", 5.f);
        aliasFloat("fSneakSpeedMultiplier", "fMoveSneakMult", 0.6f);
        aliasFloat("fHoldBreathTime", "fActorSwimBreathBase", 4.f);

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

        static const std::array nativeSkillIds{ ESM::Skill::Armorer, ESM::Skill::Athletics,
            ESM::Skill::LongBlade, ESM::Skill::Block, ESM::Skill::BluntWeapon, ESM::Skill::HandToHand,
            ESM::Skill::HeavyArmor, ESM::Skill::Alchemy, ESM::Skill::Alteration, ESM::Skill::Conjuration,
            ESM::Skill::Destruction, ESM::Skill::Illusion, ESM::Skill::Mysticism, ESM::Skill::Restoration,
            ESM::Skill::Acrobatics, ESM::Skill::LightArmor, ESM::Skill::Marksman, ESM::Skill::Mercantile,
            ESM::Skill::Security, ESM::Skill::Sneak, ESM::Skill::Speechcraft };
        static const std::array skillAttributes{ ESM::Attribute::Endurance, ESM::Attribute::Speed,
            ESM::Attribute::Strength, ESM::Attribute::Endurance, ESM::Attribute::Strength,
            ESM::Attribute::Strength, ESM::Attribute::Endurance, ESM::Attribute::Intelligence,
            ESM::Attribute::Willpower, ESM::Attribute::Intelligence, ESM::Attribute::Willpower,
            ESM::Attribute::Personality, ESM::Attribute::Intelligence, ESM::Attribute::Willpower,
            ESM::Attribute::Speed, ESM::Attribute::Speed, ESM::Attribute::Agility, ESM::Attribute::Personality,
            ESM::Attribute::Agility, ESM::Attribute::Agility, ESM::Attribute::Personality };
        for (std::size_t nativeIndex = 0; nativeIndex < nativeSkillIds.size(); ++nativeIndex)
        {
            const ESM::RefId id(nativeSkillIds[nativeIndex]);
            if (store.get<ESM::Skill>().search(id) == nullptr)
            {
                ESM::Skill skill;
                skill.blank();
                skill.mId = *id.getIf<ESM::SkillId>();
                skill.mName = ESM4::playerSkillName(nativeIndex);
                skill.mData.mAttribute = ESM::RefId(skillAttributes[nativeIndex]);
                skill.mData.mSpecialization = nativeIndex <= 6 ? ESM::Class::Combat
                    : (nativeIndex <= 13 ? ESM::Class::Magic : ESM::Class::Stealth);
                store.insertStatic(skill);
            }
        }
        // Several shared mechanics caches are indexed by the complete TES3 skill enum. Keep its six non-Oblivion
        // slots available as internal records; the player projection, class data, ObScript actor values, and M12
        // save schema expose only the 21 native skills above.
        for (int i = 0; i < ESM::Skill::Length; ++i)
        {
            const ESM::RefId id = ESM::Skill::indexToRefId(i);
            if (store.get<ESM::Skill>().search(id) != nullptr)
                continue;
            ESM::Skill skill;
            skill.blank();
            skill.mId = *id.getIf<ESM::SkillId>();
            skill.mData.mAttribute = ESM::Attribute::Luck;
            skill.mData.mSpecialization = ESM::Class::Combat;
            store.insertStatic(skill);
        }

        for (const ESM4::Race& source : store.get<ESM4::Race>())
            store.insertStatic(projectRace(source));
        for (const ESM4::Class& source : store.get<ESM4::Class>())
            store.insertStatic(projectClass(source));
        for (const ESM4::BirthSign& source : store.get<ESM4::BirthSign>())
            store.insertStatic(projectBirthSign(source));

        // M13 projects item definitions only. The mutable state remains in
        // InventoryStore/RuntimeState, while all names, icons, models, values,
        // weights, and category semantics continue to originate in TES4.
        for (const ESM4::Ammunition& source : store.get<ESM4::Ammunition>())
        {
            store.insertStatic(projectAmmunition(source));
            ++report.mProjectedItems;
        }
        for (const ESM4::Apparatus& source : store.get<ESM4::Apparatus>())
        {
            store.insertStatic(projectApparatus(source));
            ++report.mProjectedItems;
        }
        for (const ESM4::Armor& source : store.get<ESM4::Armor>())
        {
            store.insertStatic(projectArmor(source));
            ++report.mProjectedItems;
        }
        for (const ESM4::Book& source : store.get<ESM4::Book>())
        {
            store.insertStatic(projectBook(source));
            ++report.mProjectedItems;
        }
        for (const ESM4::Clothing& source : store.get<ESM4::Clothing>())
        {
            store.insertStatic(projectClothing(source));
            ++report.mProjectedItems;
        }
        for (const ESM4::Ingredient& source : store.get<ESM4::Ingredient>())
        {
            store.insertStatic(projectIngredient(source));
            ++report.mProjectedItems;
        }
        for (const ESM4::Key& source : store.get<ESM4::Key>())
        {
            store.insertStatic(projectMisc(source, true));
            ++report.mProjectedItems;
        }
        for (const ESM4::MiscItem& source : store.get<ESM4::MiscItem>())
        {
            if (Misc::StringUtils::ciEqual(source.mEditorId, "Lockpick"))
                store.insertStatic(projectLockpick(source));
            else if (Misc::StringUtils::ciEqual(source.mEditorId, "RepairHammer"))
                store.insertStatic(projectRepair(source));
            else
                store.insertStatic(projectMisc(source, false));
            if (Misc::StringUtils::ciEqual(source.mEditorId, "Gold001"))
            {
                ESM::Miscellaneous gold = projectMisc(source, false);
                gold.mId = ContainerStore::sGoldId;
                store.insertStatic(gold);
            }
            ++report.mProjectedItems;
        }
        for (const ESM4::Potion& source : store.get<ESM4::Potion>())
        {
            store.insertStatic(projectPotion(source));
            ++report.mProjectedItems;
        }
        for (const ESM4::SigilStone& source : store.get<ESM4::SigilStone>())
        {
            store.insertStatic(projectMisc(source, false));
            ++report.mProjectedItems;
        }
        for (const ESM4::SoulGem& source : store.get<ESM4::SoulGem>())
        {
            store.insertStatic(projectMisc(source, false));
            ++report.mProjectedItems;
        }
        for (const ESM4::Light& source : store.get<ESM4::Light>())
        {
            if ((source.mData.flags & ESM4::Light::Carryable) == 0)
                continue;
            store.insertStatic(projectLight(source));
            ++report.mProjectedItems;
        }
        for (const ESM4::Weapon& source : store.get<ESM4::Weapon>())
        {
            store.insertStatic(projectWeapon(source));
            ++report.mProjectedItems;
        }

        const ESM4::Npc& nativePlayer = findPlayer(store);
        report.mPlayerSource = nativePlayer.mEditorId + "@" + nativePlayer.mId.toString();
        const ESM4::Race* nativeRace = store.get<ESM4::Race>().search(ESM::RefId(nativePlayer.mRace));
        const ESM4::Class* nativeClass = store.get<ESM4::Class>().search(ESM::RefId(nativePlayer.mClass));

        const ESM::RefId raceId
            = nativeRace != nullptr ? ESM::RefId(nativeRace->mId) : ESM::RefId::stringRefId("OpenMWOblivionPlayerRace");
        const std::array<unsigned char, ESM::Attribute::Length> maleRaceAttributes
            = nativeRace != nullptr ? attributes(nativeRace->mAttribMale)
                                    : std::array<unsigned char, ESM::Attribute::Length>{ 40, 40, 40, 40, 40, 40, 40, 40 };
        const std::array<unsigned char, ESM::Attribute::Length> femaleRaceAttributes
            = nativeRace != nullptr ? attributes(nativeRace->mAttribFemale) : maleRaceAttributes;
        if (nativeRace == nullptr)
        {
            ESM::Race race;
            race.blank();
            race.mId = raceId;
            race.mName = "Imperial";
            race.mData.mFlags = ESM::Race::Playable;
            for (int i = 0; i < ESM::Attribute::Length; ++i)
            {
                race.mData.setAttribute(ESM::Attribute::indexToRefId(i), true, maleRaceAttributes[i]);
                race.mData.setAttribute(ESM::Attribute::indexToRefId(i), false, femaleRaceAttributes[i]);
            }
            store.insertStatic(race);
        }
        report.mRaceSource = nativeRace != nullptr ? nativeRace->mEditorId + "@" + nativeRace->mId.toString() : "default";

        const ESM::RefId classId = nativeClass != nullptr ? ESM::RefId(nativeClass->mId)
                                                          : ESM::RefId::stringRefId("OpenMWOblivionPlayerClass");
        if (nativeClass == nullptr)
        {
            ESM::Class characterClass;
            characterClass.blank();
            characterClass.mId = classId;
            characterClass.mName = "Adventurer";
            characterClass.mData.mAttribute = { ESM::Attribute::Strength, ESM::Attribute::Endurance };
            characterClass.mData.mSpecialization = ESM::Class::Combat;
            characterClass.mData.mIsPlayable = 1;
            store.insertStatic(characterClass);
        }
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
        for (int i = 0; i < ESM::Skill::Length; ++i)
            player.mNpdt.mSkills[ESM::Skill::indexToRefId(i)] = 5;
        if (nativeRace != nullptr && nativeClass != nullptr)
        {
            const ESM4::PlayerCharacterStats built = ESM4::buildPlayerCharacterStats(
                *nativeRace, *nativeClass, nullptr, !player.isMale());
            for (int i = 0; i < ESM::Attribute::Length; ++i)
                player.mNpdt.mAttributes[ESM::Attribute::indexToRefId(i)]
                    = static_cast<unsigned char>(std::clamp(built.mAttributes[i], 0.f, 255.f));
            for (std::size_t i = 0; i < nativeSkillIds.size(); ++i)
                player.mNpdt.mSkills[ESM::RefId(nativeSkillIds[i])]
                    = static_cast<unsigned char>(std::clamp(built.mSkills[i], 0.f, 255.f));
            player.mNpdt.mHealth = static_cast<std::uint16_t>(std::clamp(built.mHealth, 1.f, 65535.f));
            player.mNpdt.mMana = static_cast<std::uint16_t>(std::clamp(built.mMagicka, 1.f, 65535.f));
            player.mNpdt.mFatigue = static_cast<std::uint16_t>(std::clamp(built.mFatigue, 1.f, 65535.f));
            Log(Debug::Info) << "M12 player stats: race=" << player.mRace << " class=" << player.mClass
                             << " sex=" << (player.isMale() ? "male" : "female") << " health=" << built.mHealth
                             << " magicka=" << built.mMagicka << " fatigue=" << built.mFatigue
                             << " capacity=" << built.mCapacity << " breath=" << built.mBreathTime;
        }
        else
        {
            // DATA is packed: its 32-bit Health follows 21 skill bytes.
            // std::clamp takes references, so first copy to aligned storage.
            const std::uint32_t health = nativePlayer.mData.health;
            player.mNpdt.mHealth
                = static_cast<std::uint16_t>(std::clamp<std::uint32_t>(health, 1, 65535));
            player.mNpdt.mFatigue = std::max<std::uint16_t>(1, nativePlayer.mBaseConfig.tes4.fatigue);
            const auto playerAttributes = attributes(nativePlayer.mData.attribs);
            for (int i = 0; i < ESM::Attribute::Length; ++i)
                player.mNpdt.mAttributes[ESM::Attribute::indexToRefId(i)]
                    = playerAttributes[i] != 0 ? playerAttributes[i] : maleRaceAttributes[i];
            player.mNpdt.mMana = std::max<std::uint16_t>(
                1, static_cast<std::uint16_t>(player.mNpdt.mAttributes[ESM::Attribute::Intelligence] * 2));
        }
        for (const ESM4::InventoryItem& item : nativePlayer.mInventory)
        {
            const ESM::RefId nativeId(ESM::FormId::fromUint32(item.item));
            const ESM::RefId id = sharedItemId(store, nativeId);
            // The generic static-record index is finalized later in ESMStore setup.  The native
            // definition registry is already complete here and is the authoritative projection gate.
            if (item.count != 0 && itemDefinition(store, nativeId).has_value())
            {
                static_cast<void>(ESM4::inventoryItemCount(item));
                player.mInventory.mList.push_back(
                    { item.count, id });
            }
        }
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
        Settings::camera().mFieldOfView.set(75.f);
        Settings::camera().mFirstPersonFieldOfView.set(75.f);

        Log(Debug::Info) << "Installed Oblivion profile services: " << report.mNativeGameSettings
                         << " native GMSTs, " << report.mRuntimeContractSettings << " reviewed runtime settings, "
                         << report.mNativeGlobals << " native globals, " << report.mProjectedItems
                         << " native items, player " << report.mPlayerSource << ", race " << report.mRaceSource
                         << ", class " << report.mClassSource;
        return report;
    }
}
