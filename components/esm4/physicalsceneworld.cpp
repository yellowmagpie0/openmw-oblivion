#include "physicalsceneworld.hpp"

#include <cmath>
#include <stdexcept>

namespace ESM4
{
    namespace
    {
        template <std::size_t N>
        void finite(const std::array<float, N>& values)
        {
            for (float value : values)
                if (!std::isfinite(value))
                    throw std::invalid_argument("nonfinite native World scene input/result");
        }

        void rotation(const std::array<float, 4>& value)
        {
            finite(value);
            double squared = 0;
            for (float component : value)
                squared += double(component) * component;
            if (std::abs(squared - 1.0) > 1e-4)
                throw std::invalid_argument("invalid native World scene unit rotation");
        }

        float dot4(const std::array<float, 4>& a, const std::array<float, 4>& b)
        {
            const float x = a[0] * b[0], y = a[1] * b[1];
            const float z = a[2] * b[2], w = a[3] * b[3];
            const float xz = z + x, yw = w + y;
            return yw + xz;
        }

        float reciprocalRoot(float squared)
        {
            if (!std::isfinite(squared) || squared <= 0.f)
                throw std::invalid_argument("invalid native World scene normalization");
            // Portable estimate followed by the original single Newton step.
            const float reciprocal = 1.f / std::sqrt(squared);
            const float first = squared * reciprocal;
            const float second = first * reciprocal;
            const float error = 3.f - second;
            const float half = .5f * reciprocal;
            return half * error;
        }
    }

    std::optional<PhysicalWorldSceneVelocities> preparePhysicalWorldSceneVelocities(
        const PhysicalWorldSceneInput& input)
    {
        if (!input.mHasWorld || !input.mHasAuthority)
            return std::nullopt;
        finite(input.mCurrentOrigin);
        rotation(input.mCurrentRotation);
        rotation(input.mTargetRotation);
        std::array<float, 3> displacement;
        for (std::size_t i = 0; i < 3; ++i)
        {
            if (!std::isfinite(input.mTargetPosition[i]))
                throw std::invalid_argument("nonfinite native World scene target position");
            displacement[i] = input.mTargetPosition[i] - input.mCurrentOrigin[i];
        }
        const float x = displacement[0] * displacement[0];
        const float y = displacement[1] * displacement[1];
        const float z = displacement[2] * displacement[2];
        const float xy = y + x, distanceSquared = z + xy;
        const float dot = dot4(input.mTargetRotation, input.mCurrentRotation);
        const float rotationDifference = dot - 1.f;
        // Native comparisons are strict. The first rotation test keeps signs;
        // physically equivalent negative quaternions still take the drive path.
        if (distanceSquared <= 1.0000001111620804e-6f
            && std::abs(rotationDifference) <= .0010000000474974513f)
            return PhysicalWorldSceneVelocities{};
        if (!std::isfinite(input.mFrameSeconds) || input.mFrameSeconds < 0.f)
            throw std::invalid_argument("invalid prepared native World scene frame");
        if (input.mFrameSeconds == 0.f)
            return std::nullopt;
        const float inverseFrame = float(1.0 / double(input.mFrameSeconds));
        if (!std::isfinite(inverseFrame))
            throw std::invalid_argument("native World scene inverse frame overflow");
        finite(input.mLocalCenter);
        finite(input.mEndCenter);
        finite(input.mTargetPosition);
        const auto& q = input.mTargetRotation;
        const auto& local = input.mLocalCenter;
        const float px = q[0] * local[0], py = q[1] * local[1], pz = q[2] * local[2];
        const float pxy = py + px, projection = pz + pxy;
        const float axisCoefficient = float(2.0 * double(projection));
        const float scalarCoefficient = float(2.0 * double(q[3]) * q[3] - 1.0);
        const float crossCoefficient = float(2.0 * double(q[3]));
        PhysicalWorldSceneVelocities result{};
        for (std::size_t i = 0; i < 4; ++i)
        {
            float cross = 0.f;
            if (i < 3)
            {
                const auto next = (i + 1) % 3, last = (i + 2) % 3;
                const float first = q[next] * local[last], second = q[last] * local[next];
                cross = first - second;
            }
            const float crossTerm = cross * crossCoefficient;
            const float axisTerm = axisCoefficient * (i < 3 ? q[i] : 0.f);
            const float scalarTerm = scalarCoefficient * local[i];
            const float sum = axisTerm + scalarTerm;
            const float rotated = crossTerm + sum;
            const float desired = rotated + input.mTargetPosition[i];
            const float delta = desired - input.mEndCenter[i];
            result.mLinear[i] = inverseFrame * delta;
        }
        // Original8A2B40: desired * conjugate(current), including its paired
        // SSE dot reduction for the fourth lane. Do not replace with library
        // quaternion multiplication or a differently grouped scalar sum.
        const auto& current = input.mCurrentRotation;
        std::array<float, 4> relative;
        for (std::size_t i = 0; i < 3; ++i)
        {
            const auto next = (i + 1) % 3, last = (i + 2) % 3;
            const float first = q[last] * current[next], second = current[last] * q[next];
            const float cross = first - second;
            const float firstTerm = q[3] * current[i];
            const float difference = cross - firstTerm;
            const float secondTerm = current[3] * q[i];
            relative[i] = secondTerm + difference;
        }
        relative[3] = dot;
        const float normalize = reciprocalRoot(dot4(relative, relative));
        for (float& component : relative)
            component *= normalize;
        const float absoluteW = std::abs(relative[3]);
        const float halfAngle = absoluteW < 1.f ? float(std::acos(double(absoluteW))) : 0.f;
        const float angle = halfAngle * 2.f;
        if (angle > .0010000000474974513f)
        {
            const float ax = relative[0] * relative[0], ay = relative[1] * relative[1];
            const float az = relative[2] * relative[2], axy = ay + ax;
            const float factor = reciprocalRoot(az + axy);
            const float speed = float(double(angle) * double(inverseFrame));
            for (std::size_t i = 0; i < 4; ++i)
            {
                float axis = relative[i] * factor;
                if (relative[3] < 0.f)
                    axis = -axis;
                result.mAngular[i] = speed * axis;
            }
        }
        finite(result.mLinear);
        finite(result.mAngular);
        return result;
    }
}
