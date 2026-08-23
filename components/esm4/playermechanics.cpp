#include "playermechanics.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>

#include <components/misc/constants.hpp>

#include "actor.hpp"
#include "loadbsgn.hpp"
#include "loadclas.hpp"
#include "loadrace.hpp"

namespace
{
    std::array<float, 8> attributes(const ESM4::AttributeValues& value)
    {
        return { static_cast<float>(value.strength), static_cast<float>(value.intelligence),
            static_cast<float>(value.willpower), static_cast<float>(value.agility), static_cast<float>(value.speed),
            static_cast<float>(value.endurance), static_cast<float>(value.personality), static_cast<float>(value.luck) };
    }

    std::string lower(std::string_view value)
    {
        std::string result(value);
        std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    void applyBirthSign(ESM4::PlayerCharacterStats& result, const ESM4::BirthSign* sign)
    {
        if (sign == nullptr)
            return;
        std::string id = lower(sign->mEditorId);
        if (id.starts_with("birthsign"))
            id.erase(0, std::string_view("birthsign").size());
        if (id == "warrior")
        {
            result.mAttributes[0] += 10.f;
            result.mAttributes[5] += 10.f;
        }
        else if (id == "steed")
            result.mAttributes[4] += 20.f;
        else if (id == "thief")
        {
            result.mAttributes[3] += 10.f;
            result.mAttributes[4] += 10.f;
            result.mAttributes[7] += 10.f;
        }
        else if (id == "lady")
        {
            result.mAttributes[2] += 10.f;
            result.mAttributes[5] += 10.f;
        }
        else if (id == "mage")
            result.mMagicka += 50.f;
        else if (id == "apprentice")
            result.mMagicka += 100.f;
        else if (id == "atronach")
        {
            result.mMagicka += 150.f;
            result.mStuntedMagicka = true;
        }
    }
}

int ESM4::playerSkillIndex(std::uint32_t nativeSkill)
{
    return nativeSkill >= Race::Skill_Armorer && nativeSkill <= Race::Skill_Speechcraft
        ? static_cast<int>(nativeSkill - Race::Skill_Armorer)
        : -1;
}

std::string_view ESM4::playerSkillName(std::size_t index)
{
    static constexpr std::array names{ "Armorer", "Athletics", "Blade", "Block", "Blunt", "Hand to Hand",
        "Heavy Armor", "Alchemy", "Alteration", "Conjuration", "Destruction", "Illusion", "Mysticism",
        "Restoration", "Acrobatics", "Light Armor", "Marksman", "Mercantile", "Security", "Sneak",
        "Speechcraft" };
    return index < names.size() ? names[index] : std::string_view{};
}

ESM4::PlayerCharacterStats ESM4::buildPlayerCharacterStats(
    const Race& race, const Class& characterClass, const BirthSign* birthSign, bool female)
{
    PlayerCharacterStats result;
    result.mAttributes = attributes(female ? race.mAttribFemale : race.mAttribMale);
    result.mSkills.fill(5.f);

    for (const auto& [nativeSkill, bonus] : race.mSkillBonus)
        if (const int index = playerSkillIndex(nativeSkill); index >= 0)
            result.mSkills[index] += bonus;

    for (const std::uint32_t attribute : characterClass.mData.mFavoredAttributes)
        if (attribute < result.mAttributes.size())
            result.mAttributes[attribute] += 5.f;

    for (std::size_t i = 0; i < result.mSkills.size(); ++i)
    {
        const int specialization = i <= 6 ? 0 : (i <= 13 ? 1 : 2);
        if (specialization == static_cast<int>(characterClass.mData.mSpecialization))
            result.mSkills[i] += 5.f;
    }
    for (const std::uint32_t nativeSkill : characterClass.mData.mMajorSkills)
        if (const int index = playerSkillIndex(nativeSkill); index >= 0)
            result.mSkills[index] += 20.f;

    applyBirthSign(result, birthSign);
    result.mHealth = result.mAttributes[5] * 2.f;
    result.mMagicka += result.mAttributes[1] * 2.f;
    result.mFatigue = result.mAttributes[0] + result.mAttributes[2] + result.mAttributes[3] + result.mAttributes[5];
    result.mCapacity = result.mAttributes[0] * 5.f;
    result.mBreathTime = 4.f + result.mAttributes[5] * 0.3f;
    return result;
}

float ESM4::playerEncumbranceFactor(float encumbrance, float capacity)
{
    if (capacity <= 0.f)
        return 0.f;
    return std::clamp(encumbrance / capacity, 0.f, 1.f);
}

float ESM4::playerWalkSpeed(float speed, float encumbrance, float capacity, bool sneaking)
{
    if (encumbrance > std::max(0.f, capacity))
        return 0.f;
    const float base = 90.f + std::clamp(speed, 0.f, 100.f) * 0.4f;
    return base * (1.f - 0.4f * playerEncumbranceFactor(encumbrance, capacity)) * (sneaking ? 0.6f : 1.f);
}

float ESM4::playerRunSpeed(float speed, float encumbrance, float capacity)
{
    return playerWalkSpeed(speed, encumbrance, capacity, false) * 3.f;
}

float ESM4::playerJumpHeight(float acrobatics, float encumbrance, float capacity)
{
    if (encumbrance > std::max(0.f, capacity))
        return 0.f;
    return (64.f + std::clamp(acrobatics, 0.f, 100.f)) * (1.f - 0.4f * playerEncumbranceFactor(encumbrance, capacity));
}

float ESM4::playerJumpVelocity(float acrobatics, float encumbrance, float capacity)
{
    const float height = playerJumpHeight(acrobatics, encumbrance, capacity);
    return std::sqrt(2.f * Constants::GravityConst * Constants::UnitsPerMeter * height);
}

float ESM4::playerRunFatigueCost(float seconds) { return std::max(0.f, seconds) * 8.f; }
float ESM4::playerJumpFatigueCost() { return 30.f; }
float ESM4::playerFatigueRecovery(float seconds) { return std::max(0.f, seconds) * 10.f; }
