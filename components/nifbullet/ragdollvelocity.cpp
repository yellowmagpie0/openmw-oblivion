#include "ragdollvelocity.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace NifBullet
{
    namespace
    {
        void coefficient(float value)
        {
            if (!std::isfinite(value) || value < 0)
                throw std::invalid_argument("Invalid native motion coefficient");
        }

        void finite(const osg::Vec3f& value)
        {
            for (unsigned i = 0; i < 3; ++i)
                if (!std::isfinite(value[i]))
                    throw std::invalid_argument("Nonfinite native motion velocity");
        }

        float squaredLength(const osg::Vec3f& value)
        {
            // Original SSE multiplies and reduces Y+X, then Z, with a
            // binary32 store at each operation; do not use a double dot product.
            const float x = value.x() * value.x();
            const float y = value.y() * value.y();
            const float z = value.z() * value.z();
            const float xy = y + x;
            const float result = z + xy;
            if (!std::isfinite(result))
                throw std::invalid_argument("Native motion squared velocity overflow");
            return result;
        }

        void multiply(osg::Vec3f& value, float factor)
        {
            for (unsigned i = 0; i < 3; ++i)
                value[i] *= factor;
            finite(value);
        }
    }

    osg::Vec3f ragdollNativeGravityDelta(const osg::Vec3f& gravity, float frameSeconds)
    {
        finite(gravity);
        coefficient(frameSeconds);
        osg::Vec3f result;
        for (unsigned axis = 0; axis < 3; ++axis)
            result[axis] = gravity[axis] * frameSeconds;
        finite(result);
        return result;
    }

    RagdollMotionLimits ragdollLoadedMotionLimits(float linearDamping, float angularDamping,
        float maxLinearVelocity, float maxAngularVelocity)
    {
        coefficient(linearDamping);
        coefficient(angularDamping);
        coefficient(maxLinearVelocity);
        coefficient(maxAngularVelocity);
        // 8a4513..8a4532 caps the loaded Cinfo at 250, then
        // 8a42dd..8a42fc imposes a minimum of 250 before motion construction.
        return {linearDamping, angularDamping, 250.f, maxAngularVelocity};
    }

    RagdollNativeVelocities ragdollNativeVelocityStep(const RagdollNativeVelocities& input,
        const RagdollMotionLimits& limits, float frameSeconds, const osg::Vec3f& linearDelta)
    {
        coefficient(frameSeconds);
        coefficient(limits.mLinearDamping);
        coefficient(limits.mAngularDamping);
        coefficient(limits.mMaxLinearVelocity);
        coefficient(limits.mAngularLimit);
        finite(input.mLinear);
        finite(input.mAngular);
        finite(linearDelta);
        auto result = input;
        for (unsigned i = 0; i < 3; ++i)
            result.mLinear[i] += linearDelta[i];
        finite(result.mLinear);
        multiply(result.mLinear, float(std::max(0.0, 1.0 - double(frameSeconds) * limits.mLinearDamping)));
        multiply(result.mAngular, float(std::max(0.0, 1.0 - double(frameSeconds) * limits.mAngularDamping)));
        const float linearSquared = squaredLength(result.mLinear);
        if (linearSquared > double(limits.mMaxLinearVelocity) * limits.mMaxLinearVelocity)
            multiply(result.mLinear, float(double(limits.mMaxLinearVelocity) / std::sqrt(double(linearSquared))));

        auto angularStep = result.mAngular;
        multiply(angularStep, float(double(frameSeconds) * .5));
        // Original constants and stores. Angular admission is expressed in
        // fractions of pi; its maximum step is the binary32 constant 0.9.
        const float angularSquared = float(double(squaredLength(angularStep)) * 0.40528470277786255f);
        if (!std::isfinite(angularSquared))
            throw std::invalid_argument("Native motion angular step overflow");
        const double maximum = std::min(double(limits.mAngularLimit) * frameSeconds,
            double(0.8999999761581421f));
        const float maximumSquared = float(maximum * maximum);
        if (angularSquared > maximumSquared)
            multiply(result.mAngular, float(maximum / std::sqrt(double(angularSquared))));
        return result;
    }
}
