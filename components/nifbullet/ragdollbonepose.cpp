#include "ragdollbonepose.hpp"

#include <cmath>
#include <stdexcept>

namespace NifBullet
{
    namespace
    {
        void validate(const Nif::NiTransform& pose)
        {
            if (!std::isfinite(pose.mScale) || pose.mScale <= 0)
                throw std::invalid_argument("invalid ragdoll node scale");
            for (unsigned row = 0; row < 3; ++row)
            {
                if (!std::isfinite(pose.mTranslation[row]))
                    throw std::invalid_argument("nonfinite ragdoll node position");
                for (unsigned column = 0; column < 3; ++column)
                    if (!std::isfinite(pose.mRotation.mValues[row][column]))
                        throw std::invalid_argument("nonfinite ragdoll node rotation");
            }
        }

        float store(double value)
        {
            const float result = static_cast<float>(value);
            if (!std::isfinite(result))
                throw std::invalid_argument("nonfinite ragdoll node transform result");
            return result;
        }

        osg::Vec3f rotate(const Nif::Matrix3& rotation, const osg::Vec3f& position)
        {
            osg::Vec3f result;
            for (unsigned row = 0; row < 3; ++row)
            {
                const auto& axis = rotation.mValues[row];
                result[row] = store(double(axis[0]) * position[0]
                    + double(axis[1]) * position[1] + double(axis[2]) * position[2]);
            }
            return result;
        }

        Nif::NiTransform localPose(const Nif::NiTransform& world, const Nif::NiTransform& parent)
        {
            Nif::Matrix3 inverseRotation;
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned column = 0; column < 3; ++column)
                    inverseRotation.mValues[row][column] = parent.mRotation.mValues[column][row];
            const float inverseScale = store(1.0 / double(parent.mScale));
            auto inversePosition = rotate(inverseRotation, -parent.mTranslation);
            for (unsigned axis = 0; axis < 3; ++axis)
                inversePosition[axis] = store(double(inversePosition[axis]) * inverseScale);

            Nif::NiTransform result = world;
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned column = 0; column < 3; ++column)
                    result.mRotation.mValues[row][column] = store(
                        double(inverseRotation.mValues[row][0]) * world.mRotation.mValues[0][column]
                        + double(inverseRotation.mValues[row][1]) * world.mRotation.mValues[1][column]
                        + double(inverseRotation.mValues[row][2]) * world.mRotation.mValues[2][column]);
            const auto rotated = rotate(inverseRotation, world.mTranslation);
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                const float scaled = store(double(rotated[axis]) * inverseScale);
                result.mTranslation[axis] = store(double(scaled) + inversePosition[axis]);
            }
            return result;
        }
    }

    RagdollBonePoseWriteback ragdollBonePoseWriteback(const Nif::NiTransform& previousLocal,
        const Nif::NiTransform& previousWorld, const Nif::NiTransform& desiredWorld,
        const Nif::NiTransform* parentWorld, std::uint16_t collisionFlags, bool forceWorldUpdate)
    {
        validate(previousLocal);
        validate(previousWorld);
        validate(desiredWorld);
        RagdollBonePoseWriteback result{ previousLocal, previousWorld, 0 };
        if (collisionFlags & 0x8)
        {
            if (parentWorld)
            {
                validate(*parentWorld);
                result.mLocal = localPose(desiredWorld, *parentWorld);
            }
            else
                result.mLocal = desiredWorld;
            result.mLocal.mScale = previousLocal.mScale;
        }

        if (forceWorldUpdate)
            result.mWorldChangeMask = 3;
        else
        {
            // Native delta is stored as binary32 before its inclusive compare.
            for (unsigned row = 0; row < 3; ++row)
            {
                for (unsigned column = 0; column < 3; ++column)
                    if (std::abs(previousWorld.mRotation.mValues[row][column]
                            - desiredWorld.mRotation.mValues[row][column]) > .001f)
                        result.mWorldChangeMask |= 2;
                if (std::abs(previousWorld.mTranslation[row] - desiredWorld.mTranslation[row]) > .01f)
                    result.mWorldChangeMask |= 1;
            }
        }
        if (result.mWorldChangeMask)
        {
            result.mWorld = desiredWorld;
            result.mWorld.mScale = previousWorld.mScale;
        }
        return result;
    }
}
