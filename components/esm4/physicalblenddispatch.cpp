#include "physicalblenddispatch.hpp"

#include <cmath>
#include <stdexcept>

namespace ESM4
{
    std::optional<PhysicalBlendDispatch> resolvePhysicalBlendDispatch(
        float hierarchyGain, float velocityGain, std::uint16_t collisionFlags, std::uint32_t rawUpdateSelector)
    {
        if (!std::isfinite(hierarchyGain) || !std::isfinite(velocityGain))
            throw std::invalid_argument("Nonfinite native physical blend gain");
        if (hierarchyGain == 0.f)
        {
            if (rawUpdateSelector == 1)
                return std::nullopt;
            const auto route = velocityGain == 0.f || (collisionFlags & 0x100)
                ? PhysicalBlendRoute::PhysicsToScene : PhysicalBlendRoute::PoseAndVelocity;
            return PhysicalBlendDispatch{PhysicalBlendMotion::Dynamic, route};
        }
        if (hierarchyGain == 1.f)
        {
            if (rawUpdateSelector == 2)
                return std::nullopt;
            return PhysicalBlendDispatch{PhysicalBlendMotion::Keyframed, PhysicalBlendRoute::SceneToPhysics};
        }
        return PhysicalBlendDispatch{PhysicalBlendMotion::Dynamic,
            rawUpdateSelector == 2 ? PhysicalBlendRoute::PhysicsToScene : PhysicalBlendRoute::PoseAndVelocity};
    }
}
