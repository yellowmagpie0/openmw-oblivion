#ifndef OPENMW_ESM4_PHYSICALSCENEWORLD_H
#define OPENMW_ESM4_PHYSICALSCENEWORLD_H

#include <array>
#include <optional>

namespace ESM4
{
    struct PhysicalWorldSceneInput
    {
        bool mHasWorld;
        bool mHasAuthority;
        std::array<float, 3> mCurrentOrigin;
        // Getter8A2F10 reads motion+80; caller resolves XYZW layout.
        std::array<float, 4> mCurrentRotation;
        // Getter8A2FF0 reads motion+90;8A3030 reads motion+60.
        std::array<float, 4> mLocalCenter;
        std::array<float, 4> mEndCenter;
        // Already converted native XYZ and an explicitly resolved scratch W.
        std::array<float, 4> mTargetPosition;
        std::array<float, 4> mTargetRotation;
        float mFrameSeconds;
    };

    struct PhysicalWorldSceneVelocities
    {
        std::array<float, 4> mLinear;
        std::array<float, 4> mAngular;
    };

    // Full8A3900 velocity intent after caller resolves World/authority and
    // converts its WXYZ quaternion argument. No motion mode/pose writes or caps.
    // Missing World/authority or far target at frame0 returns no velocity write;
    // a near target zeros both vectors even at frame0. Activation is caller-owned.
    // Portable reciprocal-square-root estimate with the native Newton step.
    std::optional<PhysicalWorldSceneVelocities> preparePhysicalWorldSceneVelocities(
        const PhysicalWorldSceneInput& input);
}
#endif
