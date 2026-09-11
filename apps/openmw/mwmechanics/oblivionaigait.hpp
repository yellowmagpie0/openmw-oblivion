#ifndef OPENMW_MWMECHANICS_OBLIVIONAIGAIT_H
#define OPENMW_MWMECHANICS_OBLIVIONAIGAIT_H

#include <optional>

#include <osg/Vec3f>

#include <components/esm4/aipackagedata.hpp>
#include <components/esm4/runtimestate.hpp>

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

    inline bool oblivionActorCanProactivelyYield(const ESM4::RuntimeActorAiState& state)
    {
        if (state.mRestrained || state.mActionReserved)
            return false;
        return state.mPhase == ESM4::PackagePhase::Complete
            || (state.mPhase == ESM4::PackagePhase::Wait && state.mPackage.isNull());
    }

    inline bool oblivionActorHasMovingIntent(const ESM4::RuntimeActorAiState& state)
    {
        return !state.mRestrained && state.mHasDestination
            && state.mPhase == ESM4::PackagePhase::Path;
    }

    inline std::optional<osg::Vec3f> oblivionProactiveYieldDirection(const osg::Vec3f& idlePosition,
        const osg::Vec3f& movingPosition, const std::optional<osg::Vec3f>& movingDestination,
        const std::optional<osg::Vec3f>& idleDestination)
    {
        osg::Vec3f direction = idlePosition - movingPosition;
        direction.z() = 0.f;
        if (direction.normalize() <= 0.f)
            return std::nullopt;

        if (movingDestination)
        {
            osg::Vec3f routeDirection = *movingDestination - movingPosition;
            routeDirection.z() = 0.f;
            if (routeDirection.normalize() > 0.f)
            {
                osg::Vec3f lateral(routeDirection.y(), -routeDirection.x(), 0.f);
                if (lateral.x() * direction.x() + lateral.y() * direction.y() < 0.f)
                    lateral = -lateral;
                direction += lateral;
            }
        }

        if (idleDestination)
        {
            osg::Vec3f destinationDirection = *idleDestination - idlePosition;
            destinationDirection.z() = 0.f;
            if (destinationDirection.normalize() > 0.f
                && destinationDirection.x() * direction.x()
                    + destinationDirection.y() * direction.y() > 0.f)
                direction += destinationDirection * 0.25f;
        }
        if (direction.normalize() <= 0.f)
            return std::nullopt;
        return direction;
    }
}

#endif
