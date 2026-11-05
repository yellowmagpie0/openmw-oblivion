#ifndef OPENMW_COMPONENTS_ESM4_CHARACTERGENSTATE_HPP
#define OPENMW_COMPONENTS_ESM4_CHARACTERGENSTATE_HPP

#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <components/esm/formkey.hpp>

namespace ESM4
{
    struct CharacterGenerationState
    {
        bool mTracking = false;
        // Constructor allocates zeroed history; disabling tracking releases it.
        std::optional<std::array<float, 21>> mSkillUses = std::array<float, 21>{};
        ESM::FormKey mRecommendedClass;

        void validate() const
        {
            if (ESM::FormKey::deserialize(mRecommendedClass.serialize()) != mRecommendedClass)
                throw std::invalid_argument("invalid native chargen cached class identity");
            if (mTracking && !mSkillUses)
                throw std::invalid_argument("native chargen tracking requires skill-use history");
            if (mSkillUses)
                for (const float value : *mSkillUses)
                    if (!std::isfinite(value))
                        throw std::invalid_argument("invalid native chargen skill-use history");
        }
        friend bool operator==(const CharacterGenerationState&, const CharacterGenerationState&) = default;
    };

    inline CharacterGenerationState setCharacterGenerationTracking(CharacterGenerationState state, bool enabled)
    {
        state.validate();
        state.mTracking = enabled;
        if (!enabled) state.mSkillUses.reset();
        else if (!state.mSkillUses) state.mSkillUses.emplace(); // Safe fresh re-entry after released history.
        return state;
    }

    inline CharacterGenerationState recordCharacterGenerationSkillUse(
        CharacterGenerationState state, unsigned skill, float amount)
    {
        state.validate();
        if (skill < 12 || skill > 32 || !std::isfinite(amount))
            throw std::invalid_argument("invalid native chargen skill-use event");
        if (state.mTracking)
        {
            float& value = (*state.mSkillUses)[skill - 12];
            value += amount;
            state.validate();
        }
        return state;
    }
}
#endif
