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

    float ragdollNativeInverseMass(float mass)
    {
        coefficient(mass);
        if (mass == 0.f)
            return 0.f;
        const float result = float(1.0 / double(mass));
        coefficient(result);
        return result;
    }

    std::array<float, 4> ragdollNativeLinearVelocityAfterForce(const std::array<float, 4>& current,
        float inverseMass, float frameSeconds, const std::array<float, 4>& force)
    {
        coefficient(inverseMass);
        coefficient(frameSeconds);
        std::array<float, 4> result;
        for (std::size_t i = 0; i < result.size(); ++i)
        {
            if (!std::isfinite(current[i]) || !std::isfinite(force[i]))
                throw std::invalid_argument("nonfinite native linear force input");
            const float frameForce = frameSeconds * force[i];
            const float delta = inverseMass * frameForce;
            result[i] = current[i] + delta;
            if (!std::isfinite(frameForce) || !std::isfinite(delta) || !std::isfinite(result[i]))
                throw std::invalid_argument("native linear force overflow");
        }
        return result;
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

    RagdollNativeVelocities ragdollNativeBlendVelocities(const RagdollNativeVelocities& current,
        const RagdollNativeVelocities& target, float velocityGain, float inverseFrameSeconds,
        std::optional<float> worldGravityZ)
    {
        finite(current.mLinear);
        finite(current.mAngular);
        finite(target.mLinear);
        finite(target.mAngular);
        if (!std::isfinite(velocityGain) || !std::isfinite(inverseFrameSeconds)
            || inverseFrameSeconds <= 0.f || (worldGravityZ && !std::isfinite(*worldGravityZ)))
            throw std::invalid_argument("Invalid native physical velocity blend input");
        const float remaining = 1.f - velocityGain;
        RagdollNativeVelocities result;
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            // SSE stores each product and sum independently. Do not replace
            // these with a double weighted sum or fused multiply-add.
            const float currentLinear = remaining * current.mLinear[axis];
            const float targetLinear = velocityGain * target.mLinear[axis];
            const float currentAngular = remaining * current.mAngular[axis];
            const float targetAngular = velocityGain * target.mAngular[axis];
            result.mLinear[axis] = currentLinear + targetLinear;
            result.mAngular[axis] = currentAngular + targetAngular;
        }
        finite(result.mLinear);
        finite(result.mAngular);
        if (worldGravityZ)
        {
            // Original x87 product/division/subtraction has only a final
            // binary32 store; compensation is not applied to X/Y or rotation.
            result.mLinear.z() = static_cast<float>(double(result.mLinear.z())
                - double(*worldGravityZ) * velocityGain / inverseFrameSeconds);
            finite(result.mLinear);
        }
        return result;
    }

    RagdollNativeTargetPose ragdollNativeBlendTargetPose(const RagdollNativeTargetPose& physical,
        const RagdollNativeTargetPose& animated, float hierarchyGain)
    {
        finite(physical.mPosition);
        finite(animated.mPosition);
        if (!std::isfinite(hierarchyGain))
            throw std::invalid_argument("Nonfinite native hierarchy gain");
        const auto validate = [](const std::array<float, 4>& rotation) {
            double squared = 0;
            for (float value : rotation)
            {
                if (!std::isfinite(value))
                    throw std::invalid_argument("Nonfinite native blend quaternion");
                squared += double(value) * value;
            }
            if (std::abs(squared - 1.0) > 1e-4)
                throw std::invalid_argument("Invalid native blend unit quaternion");
        };
        validate(physical.mRotation);
        validate(animated.mRotation);
        RagdollNativeTargetPose result;
        const float remaining = 1.f - hierarchyGain;
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            const float physicalTerm = remaining * physical.mPosition[axis];
            const float animatedTerm = hierarchyGain * animated.mPosition[axis];
            result.mPosition[axis] = physicalTerm + animatedTerm;
        }
        finite(result.mPosition);
        const auto& from = physical.mRotation;
        const auto& to = animated.mRotation;
        const float x = from[0] * to[0];
        const float y = from[1] * to[1];
        const float z = from[2] * to[2];
        const float w = from[3] * to[3];
        const float xz = z + x;
        const float yw = w + y;
        const float dot = xz + yw;
        const float sign = dot < 0.f ? -1.f : 1.f;
        const float absoluteDot = std::abs(dot);
        float physicalWeight = remaining;
        float animatedWeight = sign * hierarchyGain;
        if (absoluteDot < .9990000128746033f)
        {
            // Original8B1C60 keeps acos/FSIN and reciprocal-sine weights
            // in x87 until their binary32 stores; angle is not stored as float.
            const double angle = std::acos(double(absoluteDot));
            const double animatedAngle = angle * hierarchyGain;
            const double physicalAngle = angle - animatedAngle;
            // FSIN outside +/-2^63 leaves its operand unchanged and raises C2.
            // That unsupported original numeric domain must not silently use
            // portable libm's argument reduction and publish a different pose.
            if (std::abs(animatedAngle) >= 0x1p63 || std::abs(physicalAngle) >= 0x1p63)
                throw std::invalid_argument("Native blend trigonometric argument outside supported domain");
            const double reciprocalSine = 1.0 / std::sqrt(1.0 - double(absoluteDot) * absoluteDot);
            physicalWeight = static_cast<float>(std::sin(physicalAngle) * reciprocalSine);
            animatedWeight = static_cast<float>(std::sin(animatedAngle) * reciprocalSine * sign);
        }
        for (unsigned axis = 0; axis < 4; ++axis)
        {
            const float physicalTerm = from[axis] * physicalWeight;
            const float animatedTerm = to[axis] * animatedWeight;
            result.mRotation[axis] = physicalTerm + animatedTerm;
        }
        // Actual slerp normalizes once;88F5DF calls4D6830 a second time.
        // Preserve both Newton refinement/store sequences using a portable
        // reciprocal square root rather than CPU-specific RSQRT approximation.
        for (unsigned pass = 0; pass < 2; ++pass)
        {
            const auto& q = result.mRotation;
            const float xx = q[0] * q[0];
            const float yy = q[1] * q[1];
            const float zz = q[2] * q[2];
            const float ww = q[3] * q[3];
            const float xxzz = zz + xx;
            const float yyww = ww + yy;
            const float squared = yyww + xxzz;
            if (!std::isfinite(squared) || squared <= 0.f)
                throw std::invalid_argument("Nonrepresentable native blend quaternion");
            const float reciprocal = 1.f / std::sqrt(squared);
            const float first = squared * reciprocal;
            const float second = first * reciprocal;
            const float error = 3.f - second;
            const float half = .5f * reciprocal;
            const float factor = half * error;
            for (float& value : result.mRotation)
                value *= factor;
        }
        return result;
    }

    RagdollNativeVelocities ragdollNativeTargetVelocities(const osg::Vec3f& localCenterOfMass,
        const osg::Vec3f& currentCenterOfMass, const std::array<float, 4>& currentRotation,
        const RagdollNativeTargetPose& target, float inverseFrameSeconds,
        float maximumLinearVelocity, float maximumAngularVelocity)
    {
        finite(localCenterOfMass);
        finite(currentCenterOfMass);
        finite(target.mPosition);
        coefficient(maximumLinearVelocity);
        coefficient(maximumAngularVelocity);
        if (!std::isfinite(inverseFrameSeconds) || inverseFrameSeconds <= 0.f)
            throw std::invalid_argument("Invalid native target inverse frame time");
        const auto validateRotation = [](const std::array<float, 4>& rotation) {
            double length = 0;
            for (const float value : rotation)
            {
                if (!std::isfinite(value))
                    throw std::invalid_argument("Nonfinite native target quaternion");
                length += double(value) * value;
            }
            if (!(length > 0) || std::abs(length - 1.0) > 1e-4)
                throw std::invalid_argument("Invalid native target unit quaternion");
        };
        validateRotation(currentRotation);
        validateRotation(target.mRotation);
        const auto& q = target.mRotation;
        const osg::Vec3f qv(q[0], q[1], q[2]);
        const float dotX = q[0] * localCenterOfMass.x();
        const float dotY = q[1] * localCenterOfMass.y();
        const float dotZ = q[2] * localCenterOfMass.z();
        const float dotXY = dotY + dotX;
        const float dot = dotZ + dotXY;
        const float axisCoefficient = static_cast<float>(2.0 * dot);
        const float scalarCoefficient = static_cast<float>(2.0 * double(q[3]) * q[3] - 1.0);
        const float crossCoefficient = static_cast<float>(2.0 * q[3]);
        RagdollNativeVelocities result{};
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            const unsigned next = (axis + 1) % 3;
            const unsigned last = (axis + 2) % 3;
            const float first = q[next] * localCenterOfMass[last];
            const float second = q[last] * localCenterOfMass[next];
            const float cross = first - second;
            const float crossTerm = cross * crossCoefficient;
            const float axisTerm = axisCoefficient * qv[axis];
            const float scalarTerm = scalarCoefficient * localCenterOfMass[axis];
            const float sum = axisTerm + scalarTerm;
            const float rotated = crossTerm + sum;
            const float desired = rotated + target.mPosition[axis];
            const float displacement = desired - currentCenterOfMass[axis];
            result.mLinear[axis] = inverseFrameSeconds * displacement;
        }
        const auto cap = [](osg::Vec3f& velocity, float maximum) {
            const float length = std::sqrt(squaredLength(velocity));
            if (length > maximum)
                multiply(velocity, static_cast<float>(double(maximum) / length));
        };
        cap(result.mLinear, maximumLinearVelocity);
        // Desired * conjugate(current), preserving original SSE product order.
        const std::array<float, 4> inverse{-currentRotation[0], -currentRotation[1],
            -currentRotation[2], currentRotation[3]};
        std::array<float, 4> relative;
        float products[3];
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            const unsigned next = (axis + 1) % 3;
            const unsigned last = (axis + 2) % 3;
            const float first = inverse[last] * q[next];
            const float second = inverse[next] * q[last];
            const float cross = first - second;
            const float firstTerm = q[3] * inverse[axis];
            const float secondTerm = inverse[3] * q[axis];
            const float sum = firstTerm + cross;
            relative[axis] = secondTerm + sum;
            products[axis] = inverse[axis] * q[axis];
        }
        const float productXY = products[1] + products[0];
        const float productXYZ = products[2] + productXY;
        relative[3] = static_cast<float>(double(q[3]) * inverse[3] - productXYZ);
        const float x = relative[0] * relative[0];
        const float y = relative[1] * relative[1];
        const float z = relative[2] * relative[2];
        const float w = relative[3] * relative[3];
        const float xz = z + x;
        const float yw = w + y;
        const float squared = yw + xz;
        if (!std::isfinite(squared) || squared <= 0.f)
            throw std::invalid_argument("Invalid native relative quaternion");
        const float reciprocal = 1.f / std::sqrt(squared);
        const float first = squared * reciprocal;
        const float second = first * reciprocal;
        const float error = 3.f - second;
        const float half = .5f * reciprocal;
        const float normalized = half * error;
        for (float& value : relative)
            value *= normalized;
        const float absoluteW = std::abs(relative[3]);
        const float halfAngle = absoluteW < 1.f ? static_cast<float>(std::acos(double(absoluteW))) : 0.f;
        const float angle = halfAngle * 2.f;
        if (angle > 0.0010000000474974513)
        {
            osg::Vec3f axis(relative[0], relative[1], relative[2]);
            const float axisSquared = squaredLength(axis);
            const float inverseLength = 1.f / std::sqrt(axisSquared);
            const float firstAxis = axisSquared * inverseLength;
            const float secondAxis = firstAxis * inverseLength;
            const float axisError = 3.f - secondAxis;
            const float axisHalf = .5f * inverseLength;
            const float factor = axisHalf * axisError;
            multiply(axis, factor);
            if (relative[3] < 0.f)
                axis = -axis;
            const float angularSpeed = static_cast<float>(double(angle) * inverseFrameSeconds);
            for (unsigned i = 0; i < 3; ++i)
                result.mAngular[i] = angularSpeed * axis[i];
        }
        cap(result.mAngular, maximumAngularVelocity);
        finite(result.mLinear);
        finite(result.mAngular);
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
