#include "potionattributes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace ESM4
{
    PotionAttributeStep advancePotionAttributes(float elapsed, float lifetime, float duration)
    {
        if (!std::isfinite(lifetime) || lifetime <= 0 || !std::isfinite(elapsed) || elapsed < 0
            || elapsed >= lifetime || !std::isfinite(duration) || duration < 0)
            throw std::invalid_argument("invalid native potion attribute lifetime");
        const float next = static_cast<float>(std::min(double(lifetime), double(elapsed) + duration));
        return {next, next >= lifetime};
    }

    void PotionAttributeState::validate() const
    {
        if (!mPotion.isContent() || ESM::FormKey::deserialize(mPotion.serialize()) != mPotion
            || mModifiers.empty() || mModifiers.size() > 8)
            throw std::invalid_argument("invalid native potion attribute source");
        advancePotionAttributes(mElapsed, mDuration, 0.f);
        std::array<bool, 8> seen{};
        for (const auto& modifier : mModifiers)
        {
            if (modifier.mAttribute >= seen.size() || seen[modifier.mAttribute]
                || !std::isfinite(modifier.mDelta) || modifier.mDelta == 0.f)
                throw std::invalid_argument("invalid native potion attribute contribution");
            seen[modifier.mAttribute] = true;
        }
    }
}
