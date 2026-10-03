#ifndef OPENMW_ESM4_PHYSICALBLENDDISPATCH_H
#define OPENMW_ESM4_PHYSICALBLENDDISPATCH_H

#include <cstdint>
#include <optional>

namespace ESM4
{
    enum class PhysicalBlendMotion : std::uint8_t
    {
        Dynamic = 1,
        Keyframed = 6,
    };

    enum class PhysicalBlendRoute : std::uint8_t
    {
        PoseAndVelocity = 0,
        PhysicsToScene = 1,
        SceneToPhysics = 2,
    };

    struct PhysicalBlendDispatch
    {
        PhysicalBlendMotion mMotion;
        PhysicalBlendRoute mRoute;
        friend bool operator==(const PhysicalBlendDispatch&, const PhysicalBlendDispatch&) = default;
    };

    // Original88F3FE selects motion/route before publishing either. An absent
    // result skips this update. Gains are finite but not clamped. The raw
    // selector remains uninterpreted; only original values1/2 are special.
    // This does not switch body modes, sync poses or issue velocity drives.
    std::optional<PhysicalBlendDispatch> resolvePhysicalBlendDispatch(
        float hierarchyGain, float velocityGain, std::uint16_t collisionFlags, std::uint32_t rawUpdateSelector);
}

#endif
