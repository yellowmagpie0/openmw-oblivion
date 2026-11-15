#ifndef OPENMW_ESM4_SKILLTRAINING_H
#define OPENMW_ESM4_SKILLTRAINING_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace ESM4
{
    struct SkillTrainingSettings
    {
        float mFactor = 1.f;
        float mExponent = 1.f;
        float mMajorMultiplier = .75f;
        float mMinorMultiplier = 1.25f;
        float mSpecializationMultiplier = .75f;
    };

    inline float skillExperienceRequirement(std::int32_t base, bool major, bool specialized,
        const SkillTrainingSettings& settings)
    {
        for (const float value : {settings.mFactor, settings.mExponent, settings.mMajorMultiplier,
                 settings.mMinorMultiplier, settings.mSpecializationMultiplier})
            if (!std::isfinite(value) || value < 0.f)
                throw std::invalid_argument("invalid native skill experience setting");
        if (base < 0)
            throw std::invalid_argument("negative native skill base for training");
        const float power = std::pow(static_cast<float>(base) * settings.mFactor, settings.mExponent);
        const float required = power * (specialized ? settings.mSpecializationMultiplier : 1.f)
            * (major ? settings.mMajorMultiplier : settings.mMinorMultiplier);
        if (!std::isfinite(required))
            throw std::invalid_argument("native skill experience requirement overflow");
        return required > 0.f ? required : 1.f;
    }

    struct ScriptedSkillTraining
    {
        std::int32_t mBase;
        float mExperience;
    };

    // Ordinary use consumes fractional experience at each level and stops at 100.
    inline ScriptedSkillTraining trainUsedSkill(std::int32_t base, float experience, float earned,
        bool major, bool specialized, const SkillTrainingSettings& settings)
    {
        if (base < 0 || !std::isfinite(experience) || experience < 0.f
            || !std::isfinite(earned) || earned < 0.f || !std::isfinite(experience + earned))
            throw std::invalid_argument("invalid native ordinary skill use");
        skillExperienceRequirement(base, major, specialized, settings);
        if (base >= 100 || earned == 0.f) return {base, experience};
        experience += earned;
        while (base < 100)
        {
            const float required = skillExperienceRequirement(base, major, specialized, settings);
            if (experience < required) break;
            experience -= required;
            ++base;
        }
        return {base, base == 100 ? 0.f : experience};
    }

    // Reading a new skill book preserves earned XP and stops at the skill cap.
    // History and advancement ownership are committed by the World together.
    inline ScriptedSkillTraining trainBookSkill(std::int32_t base, float experience)
    {
        if (base < 0 || !std::isfinite(experience) || experience < 0.f)
            throw std::invalid_argument("invalid native skill-book training");
        return {base < 100 ? base + 1 : base, experience};
    }

    // Positive/zero ModPCSkill levels; no ordinary-use skill-100 cap. The caller
    // owns native publication and per-skill/attribute/specialization/major counts.
    inline ScriptedSkillTraining trainScriptedSkill(std::int32_t base, float experience, std::int32_t count,
        bool major, bool specialized, const SkillTrainingSettings& settings)
    {
        if (count < 0 || base < 0 || base > std::numeric_limits<std::int32_t>::max() - count
            || !std::isfinite(experience) || experience < 0.f)
            throw std::invalid_argument("invalid native scripted skill training");
        // Validate the settings even when XP or count is zero, before publication.
        skillExperienceRequirement(base, major, specialized, settings);
        for (std::int32_t i = 0; i < count && experience > 0.f; ++i)
            experience = std::max(0.f, experience - skillExperienceRequirement(base + i, major, specialized, settings));
        return {base + count, experience};
    }
}

#endif
