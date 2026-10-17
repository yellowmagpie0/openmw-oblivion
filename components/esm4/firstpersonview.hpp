#ifndef OPENMW_ESM4_FIRSTPERSONVIEW_H
#define OPENMW_ESM4_FIRSTPERSONVIEW_H

#include <components/nif/niftypes.hpp>

#include <cstdint>
#include <optional>

namespace ESM4
{
    struct FirstPersonViewSettings
    {
        float mHandFollowMultiplier = .75f;
        float mScaleSeconds = 2.f;
    };

    struct FirstPersonRootInput
    {
        float mPreviousFollow = 1.f;
        float mElapsed = 0.f;
        float mPitch = 0.f;
        float mYaw = 0.f;
        float mSecondaryYaw = 0.f;
        std::int16_t mProcessAction = -1;
        std::uint16_t mGroup1 = 0xffff;
        std::uint16_t mGroup3 = 0xffff;
    };

    struct FirstPersonRootRotation
    {
        float mFollow;
        Nif::NiTransform mRotation;
    };

    // Original660130 in the observed live007F arithmetic context. Group words
    // come from ActorAnimData slots1/3. Process state3 suppresses the temporary
    // follow1 override. Position/pivot, model scale and caller admission remain
    // renderer-owned. Invalid/nonfinite inputs or unrepresentable results reject.
    std::optional<FirstPersonRootRotation> updateFirstPersonRootRotation(
        const FirstPersonRootInput& input, const FirstPersonViewSettings& settings = {}) noexcept;
}

#endif
