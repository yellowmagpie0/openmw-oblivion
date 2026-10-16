#include "ragdollbonepose.hpp"

#include <components/nif/nativetransform.hpp>
#include <components/nif/nativetransformmath.hpp>

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
            float result;
            if (!Nif::tryStoreNativeTransformFloat(value, result))
                throw std::invalid_argument("nonfinite ragdoll node transform result");
            return result;
        }

        osg::Vec3f rotate(const Nif::Matrix3& rotation, const osg::Vec3f& position,
            const Nif::Detail::NativeTransformMath& math)
        {
            osg::Vec3f result;
            for (unsigned row = 0; row < 3; ++row)
            {
                const auto& axis = rotation.mValues[row];
                result[row] = store(math.dot(axis[0], position[0], axis[1], position[1], axis[2], position[2]));
            }
            return result;
        }

        Nif::NiTransform composePose(const Nif::NiTransform& parent, const Nif::NiTransform& local,
            bool composeScale, const Nif::Detail::NativeTransformMath& math)
        {
            Nif::NiTransform result = local;
            if (composeScale)
                result.mScale = store(math.multiply(parent.mScale, local.mScale));
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned column = 0; column < 3; ++column)
                    result.mRotation.mValues[row][column] = store(
                        math.dot(parent.mRotation.mValues[row][0], local.mRotation.mValues[0][column],
                            parent.mRotation.mValues[row][1], local.mRotation.mValues[1][column],
                            parent.mRotation.mValues[row][2], local.mRotation.mValues[2][column]));
            const auto rotated = rotate(parent.mRotation, local.mTranslation, math);
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                const float scaled = store(math.multiply(rotated[axis], parent.mScale));
                result.mTranslation[axis] = store(math.add(scaled, parent.mTranslation[axis]));
            }
            return result;
        }

        Nif::NiTransform localPose(const Nif::NiTransform& world, const Nif::NiTransform& parent,
            const Nif::Detail::NativeTransformMath& math)
        {
            Nif::NiTransform inverse;
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned column = 0; column < 3; ++column)
                    inverse.mRotation.mValues[row][column] = parent.mRotation.mValues[column][row];
            inverse.mScale = store(math.reciprocal(parent.mScale));
            inverse.mTranslation = rotate(inverse.mRotation, -parent.mTranslation, math);
            for (unsigned axis = 0; axis < 3; ++axis)
                inverse.mTranslation[axis] = store(math.multiply(inverse.mTranslation[axis], inverse.mScale));
            // The native local writer never reads the composed scale slot.
            return composePose(inverse, world, false, math);
        }
    }

    Nif::NiTransform composeRagdollBonePose(const Nif::NiTransform& parent, const Nif::NiTransform& local,
        Nif::NativeTransformPrecision precision)
    {
        validate(parent);
        validate(local);
        const auto result = Nif::tryComposeNativeTransform(parent, local, precision);
        if (!result) throw std::invalid_argument("nonfinite ragdoll node transform result");
        validate(*result);
        return *result;
    }

    RagdollBonePoseWriteback ragdollBoneFrame(const Nif::NiTransform& previousLocal,
        const Nif::NiTransform& desiredWorld, const Nif::NiTransform* parentWorld,
        std::uint16_t settledCollisionFlags, Nif::NativeTransformPrecision precision)
    {
        validate(previousLocal);
        if (!Nif::Detail::NativeTransformMath(precision).valid())
            throw std::invalid_argument("invalid ragdoll node arithmetic precision");
        const auto world = parentWorld ? composeRagdollBonePose(*parentWorld, previousLocal, precision) : previousLocal;
        // Original897510 consumes pending0x40 separately at admission. Its
        // ordinary dispatch tests bit0 before deriving a physical scene pose.
        if (!(settledCollisionFlags & 1))
            return {previousLocal, world, 0};
        return ragdollBonePoseWriteback(previousLocal, world, desiredWorld,
            parentWorld, settledCollisionFlags, false, precision);
    }

    RagdollBonePoseWriteback ragdollReferenceRootFrame(const Nif::NiTransform& previousLocal,
        const Nif::NiTransform& desiredWorld, std::uint16_t authoredCollisionFlags)
    {
        return ragdollBoneFrame(previousLocal, desiredWorld, nullptr, authoredCollisionFlags | 0xc);
    }

    RagdollBonePoseWriteback ragdollBonePoseWriteback(const Nif::NiTransform& previousLocal,
        const Nif::NiTransform& previousWorld, const Nif::NiTransform& desiredWorld,
        const Nif::NiTransform* parentWorld, std::uint16_t collisionFlags, bool forceWorldUpdate,
        Nif::NativeTransformPrecision precision)
    {
        const Nif::Detail::NativeTransformMath math(precision);
        if (!math.valid())
            throw std::invalid_argument("invalid ragdoll node arithmetic precision");
        validate(previousLocal);
        validate(previousWorld);
        validate(desiredWorld);
        RagdollBonePoseWriteback result{ previousLocal, previousWorld, 0 };
        if (collisionFlags & 0x8)
        {
            if (parentWorld)
            {
                validate(*parentWorld);
                result.mLocal = localPose(desiredWorld, *parentWorld, math);
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
