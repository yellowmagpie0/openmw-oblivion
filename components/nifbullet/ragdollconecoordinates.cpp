#include "ragdollconecoordinates.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace NifBullet
{
    namespace
    {
        void require(bool valid)
        {
            if (!valid)
                throw std::invalid_argument("Invalid ragdoll angular coordinates");
        }

        void validate(const osg::Vec3f& vector)
        {
            for (unsigned i = 0; i < 3; ++i)
                require(std::isfinite(vector[i]));
        }

        // Separate binary32 products and the x+y, then +z accumulation match
        // the original SSE vectors. Do not replace these with a double dot.
        float dot(const osg::Vec3f& a, const osg::Vec3f& b)
        {
            const float x = a.x() * b.x();
            const float y = a.y() * b.y();
            const float z = a.z() * b.z();
            const float xy = x + y;
            return xy + z;
        }

        osg::Vec3f cross(const osg::Vec3f& a, const osg::Vec3f& b)
        {
            const float yz = a.y() * b.z(), zy = a.z() * b.y();
            const float zx = a.z() * b.x(), xz = a.x() * b.z();
            const float xy = a.x() * b.y(), yx = a.y() * b.x();
            return { yz - zy, zx - xz, xy - yx };
        }

        // Original 8ecbb0 uses this bounded polynomial, rather than CRT atan2.
        // Constants are the executable's binary32 values loaded onto x87.
        float angle(float y, float x)
        {
            constexpr double epsilon = 0x1p-23;
            constexpr double quadratic = double(0.12107899785041809f);
            constexpr double cubic = double(0.0935228168964386f);
            constexpr double halfPi = double(1.5707963705062866f);
            constexpr double pi = double(3.1415927410125732f);
            const double ay = std::abs(double(y)), ax = std::abs(double(x));
            const bool steep = ay > ax;
            const double r = steep ? ax / (ay + epsilon) : ay / (ax + epsilon);
            const double squared = r * r;
            double result = r - quadratic * squared - cubic * (squared * r);
            if (steep)
                result = halfPi - result;
            if (x < 0)
                result = pi - result;
            if (y < 0)
                result = -result;
            return float(result);
        }
    }

    std::vector<RagdollAngularLimit> ragdollConeCoordinates(const RagdollConeJoint& joint,
        const RagdollJointFrame& worldA, const RagdollJointFrame& worldB)
    {
        validate(worldA.mAxis);
        validate(worldA.mPlane);
        validate(worldB.mAxis);
        validate(worldB.mPlane);
        for (float value : { joint.mConeAngle, joint.mPlaneMin, joint.mPlaneMax,
                 joint.mTwistMin, joint.mTwistMax })
            require(std::isfinite(value));
        require(joint.mConeAngle >= 0 && joint.mPlaneMin <= joint.mPlaneMax
            && joint.mTwistMin <= joint.mTwistMax);
        require(dot(worldA.mAxis, worldA.mAxis) > 0 && dot(worldB.mAxis, worldB.mAxis) > 0
            && dot(worldA.mPlane, worldA.mPlane) > 0 && dot(worldB.mPlane, worldB.mPlane) > 0);

        std::vector<RagdollAngularLimit> result;
        auto append = [&](const osg::Vec3f& axis, float minimum, float maximum, bool cone) {
            const float squared = dot(axis, axis);
            require(std::isfinite(squared));
            if (squared <= std::numeric_limits<float>::epsilon())
                return;
            const float length = float(std::sqrt(double(squared)));
            const float inverse = float(1.0 / double(length));
            const auto normal = axis * inverse;
            const float value = cone ? -angle(length, dot(worldA.mAxis, worldB.mAxis))
                                     : angle(dot(worldA.mAxis, worldB.mPlane), length);
            result.push_back({ normal, minimum, maximum, value });
        };
        append(cross(worldA.mAxis, worldB.mAxis), -joint.mConeAngle, 100, true);
        append(cross(worldA.mAxis, worldB.mPlane), joint.mPlaneMin, joint.mPlaneMax, false);

        const auto sum = worldA.mAxis + worldB.mAxis;
        const float squared = dot(sum, sum);
        require(std::isfinite(squared));
        const float length = std::sqrt(squared);
        const auto twistAxis = length > 1.0000000168623835e-16f
            ? sum * float(1.0 / double(length)) : worldB.mAxis;
        const auto perpendicular = cross(twistAxis, worldB.mPlane);
        const auto projected = cross(perpendicular, twistAxis);
        const float twist = angle(dot(perpendicular, worldA.mPlane), dot(projected, worldA.mPlane));
        result.push_back({ twistAxis, joint.mTwistMin, joint.mTwistMax, twist });
        for (const auto& row : result)
        {
            validate(row.mAxis);
            require(std::isfinite(row.mAngle));
        }
        return result;
    }
}
