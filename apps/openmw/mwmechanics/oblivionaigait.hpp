#ifndef OPENMW_MWMECHANICS_OBLIVIONAIGAIT_H
#define OPENMW_MWMECHANICS_OBLIVIONAIGAIT_H

#include <optional>

#include <components/esm4/aipackagedata.hpp>

namespace MWMechanics
{
    inline bool oblivionPackageShouldRun(ESM4::AIPackageType type, ESM4::PackageFlags flags,
        float requestedFollowDistance, std::optional<float> targetDistanceSquared, bool targetInDifferentCell)
    {
        if (flags.has(ESM4::PackageFlag::AlwaysSneak))
            return false;
        // Travel uses the ordinary walking gait unless the package asks for
        // running. Forcing Travel to run also makes an unflagged leader outrun
        // followers which are already using their legitimate catch-up gait.
        if (flags.has(ESM4::PackageFlag::AlwaysRun)
            || type == ESM4::AIPackageType::FleeNotCombat || type == ESM4::AIPackageType::Pursue)
            return true;
        if (type != ESM4::AIPackageType::Follow && type != ESM4::AIPackageType::Accompany)
            return false;

        // Catch up at the actor's ordinary run speed, then walk near the
        // package's requested spacing. Use the same 96-unit margin as target
        // rerouting; this is a native runtime policy, not a speed multiplier.
        const float distance = (requestedFollowDistance > 0.f ? requestedFollowDistance : 128.f) + 96.f;
        return targetInDifferentCell || (targetDistanceSquared && *targetDistanceSquared > distance * distance);
    }
}

#endif
