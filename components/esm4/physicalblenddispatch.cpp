#include "physicalblenddispatch.hpp"

#include <cmath>
#include <stdexcept>

namespace ESM4
{
    PhysicalBlendDriveParameters resolvePhysicalBlendDriveParameters(
        float preparedFrameSeconds, float velocityGain, std::uint16_t collisionFlags)
    {
        if (!std::isfinite(preparedFrameSeconds) || preparedFrameSeconds < 0.f || !std::isfinite(velocityGain))
            throw std::invalid_argument("Invalid native physical drive frame/gain");
        // Original88F656 treats either signed zero as inverse1. A nonzero
        // prepared frame is divided on x87 and then stored as binary32.
        const float inverse = preparedFrameSeconds == 0.f
            ? 1.f : static_cast<float>(1.0 / double(preparedFrameSeconds));
        if (!std::isfinite(inverse) || inverse <= 0.f)
            throw std::invalid_argument("Nonrepresentable native physical drive inverse time");
        return {inverse, (collisionFlags & 0x100) ? 1.f : velocityGain};
    }

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
