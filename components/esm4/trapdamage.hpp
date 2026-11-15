#ifndef OPENMW_COMPONENTS_ESM4_TRAPDAMAGE_H
#define OPENMW_COMPONENTS_ESM4_TRAPDAMAGE_H

#include <cstdint>

namespace ESM4
{
    struct TrapImpactSettings
    {
        float mDamage = 0;
        float mLevelledDamage = 0;
        float mMinimumSpeed = 0;
    };

    // Noncontinuous scripted trap impact. Speeds use game-world units/second;
    // the caller owns actual contact, contact-entry suppression, resistance
    // selection, Health/lifecycle publication and pushback. This is not melee
    // damage: weapon skill, armor and attack fatigue do not enter this rule.
    float trapImpactDamage(const TrapImpactSettings& settings, std::uint32_t actorLevel,
        float bodySpeed, float resistance);
}

#endif
