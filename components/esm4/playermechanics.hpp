#ifndef OPENMW_ESM4_PLAYERMECHANICS_H
#define OPENMW_ESM4_PLAYERMECHANICS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ESM4
{
    struct BirthSign;
    struct Class;
    struct Race;

    struct PlayerCharacterStats
    {
        std::array<float, 8> mAttributes{};
        std::array<float, 21> mSkills{};
        float mHealth = 0.f;
        float mMagicka = 0.f;
        float mFatigue = 0.f;
        float mCapacity = 0.f;
        float mBreathTime = 0.f;
        bool mStuntedMagicka = false;
    };

    PlayerCharacterStats buildPlayerCharacterStats(
        const Race& race, const Class& characterClass, const BirthSign* birthSign, bool female);

    int playerSkillIndex(std::uint32_t nativeSkill);
    std::string_view playerSkillName(std::size_t index);

    float playerEncumbranceFactor(float encumbrance, float capacity);
    float playerWalkSpeed(float speed, float encumbrance, float capacity, bool sneaking);
    float playerRunSpeed(float speed, float encumbrance, float capacity);
    float playerJumpHeight(float acrobatics, float encumbrance, float capacity);
    float playerJumpVelocity(float acrobatics, float encumbrance, float capacity);
    float playerRunFatigueCost(float seconds);
    float playerJumpFatigueCost();
    float playerFatigueRecovery(float seconds);
}

#endif
