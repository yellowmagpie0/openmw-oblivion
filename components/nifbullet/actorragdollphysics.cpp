#include "actorragdollphysics.hpp"
#include "ragdollconecoordinates.hpp"
#include "ragdollvelocity.hpp"

#include <components/esm4/physicalblenddispatch.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <type_traits>
#include <unordered_set>
#include <unordered_map>
#include <stdexcept>

#include <BulletCollision/CollisionShapes/btCompoundShape.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <BulletCollision/CollisionShapes/btMultiSphereShape.h>
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletDynamics/ConstraintSolver/btHingeConstraint.h>
#include <BulletDynamics/ConstraintSolver/btPoint2PointConstraint.h>
#include <BulletDynamics/Dynamics/btDynamicsWorld.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>

namespace NifBullet
{
    namespace
    {
        void require(bool valid, const char* message)
        {
            if (!valid)
                throw std::invalid_argument(std::string("Invalid articulated body: ") + message);
        }
        bool finite(const btVector3& value)
        {
            return std::isfinite(value.x()) && std::isfinite(value.y()) && std::isfinite(value.z());
        }
        void validatePose(const btTransform& pose)
        {
            require(finite(pose.getOrigin()), "nonfinite position");
            const auto& basis = pose.getBasis();
            for (int row = 0; row < 3; ++row)
                require(finite(basis[row]), "nonfinite rotation");
            require(std::abs(basis.determinant() - 1) < 1e-4, "nonrigid pose");
            const auto product = basis * basis.transpose();
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 3; ++col)
                    require(std::abs(product[row][col] - (row == col ? 1 : 0)) < 1e-4, "nonorthogonal pose");
        }
        btVector3 vector(const osg::Vec3f& value)
        {
            const btVector3 result(value.x(), value.y(), value.z());
            require(finite(result), "nonfinite native vector");
            return result;
        }
        void coefficient(float value)
        {
            require(std::isfinite(value) && value >= 0, "invalid coefficient");
        }
        void radius(float value)
        {
            require(std::isfinite(value) && value > 0, "invalid radius");
        }
        btTransform hingeFrame(const RagdollJointFrame& frame, btScalar scale)
        {
            auto axis = vector(frame.mAxis);
            auto plane = vector(frame.mPlane);
            require(axis.length2() > 0, "zero hinge axis");
            axis.normalize();
            plane -= axis * plane.dot(axis);
            require(plane.length2() > 0, "zero hinge plane");
            plane.normalize();
            const auto second = axis.cross(plane);
            // Bullet's hinge axis is the local Z column.
            const btMatrix3x3 basis(plane.x(), second.x(), axis.x(), plane.y(), second.y(), axis.y(),
                plane.z(), second.z(), axis.z());
            return btTransform(basis, vector(frame.mPivot) * scale);
        }

        void frictionRow(btTypedConstraint::btConstraintInfo2* info, int index,
            const btVector3& axis, btScalar maximum)
        {
            const int offset = index * info->rowskip;
            for (int component = 0; component < 3; ++component)
            {
                info->m_J1angularAxis[offset + component] = axis[component];
                info->m_J2angularAxis[offset + component] = -axis[component];
            }
            info->m_constraintError[offset] = 0;
            info->m_lowerLimit[offset] = -maximum;
            info->m_upperLimit[offset] = maximum;
        }

        class NativeHingeConstraint final : public btHingeConstraint
        {
            float mFriction, mLengthScale;
            bool mMalleable;
            float mTau, mDamping;
            int mFrictionRow = 0;

        public:
            NativeHingeConstraint(btRigidBody& a, btRigidBody& b, const btTransform& frameA,
                const btTransform& frameB, float friction, float scale, const RagdollJointDefinition& definition)
                : btHingeConstraint(a, b, frameA, frameB, true)
                , mFriction(friction)
                , mLengthScale(scale)
                , mMalleable(definition.mMalleable)
                , mTau(definition.mTau)
                , mDamping(definition.mDamping)
            {
            }

            void getInfo1(btConstraintInfo1* info) override
            {
                btHingeConstraint::getInfo1(info);
                mFrictionRow = info->m_numConstraintRows;
                if (mFriction > 0)
                    ++info->m_numConstraintRows;
            }

            void getInfo2(btConstraintInfo2* info) override
            {
                if (mMalleable)
                    info->erp = mTau;
                btHingeConstraint::getInfo2(info);
                if (mFriction > 0)
                {
                    const auto axis = (m_rbA.getCenterOfMassTransform() * getAFrame()).getBasis().getColumn(2);
                    frictionRow(info, mFrictionRow, axis,
                        ragdollFrictionImpulse(mFriction, float(1 / info->fps), mLengthScale));
                }
                if (mMalleable)
                    info->m_damping = mDamping;
            }
        };

        class NativeConeConstraint final : public btPoint2PointConstraint
        {
            RagdollConeJoint mJoint;
            btVector3 mAxisA, mPlaneA, mAxisB, mPlaneB;
            btMatrix3x3 mShapeToCenterA;
            float mLengthScale;
            bool mMalleable;
            float mTau, mDamping;
            std::vector<RagdollAngularLimit> mRows;

            static osg::Vec3f native(const btVector3& value)
            {
                return {float(value.x()), float(value.y()), float(value.z())};
            }

            std::vector<RagdollAngularLimit> coordinates() const
            {
                const auto& a = m_rbA.getCenterOfMassTransform().getBasis();
                const auto& b = m_rbB.getCenterOfMassTransform().getBasis();
                return ragdollConeCoordinates(mJoint,
                    {{0, 0, 0}, native(a * mAxisA), native(a * mPlaneA)},
                    {{0, 0, 0}, native(b * mAxisB), native(b * mPlaneB)});
            }

        public:
            NativeConeConstraint(btRigidBody& a, btRigidBody& b, const RagdollConeJoint& joint,
                const btTransform& centerA, const btTransform& centerB, btScalar scale,
                const RagdollJointDefinition& definition)
                : btPoint2PointConstraint(a, b, centerA.inverse() * (vector(joint.mA.mPivot) * scale),
                    centerB.inverse() * (vector(joint.mB.mPivot) * scale))
                , mJoint(joint)
                , mAxisA(centerA.getBasis().transpose() * vector(joint.mA.mAxis))
                , mPlaneA(centerA.getBasis().transpose() * vector(joint.mA.mPlane))
                , mAxisB(centerB.getBasis().transpose() * vector(joint.mB.mAxis))
                , mPlaneB(centerB.getBasis().transpose() * vector(joint.mB.mPlane))
                , mShapeToCenterA(centerA.getBasis().transpose())
                , mLengthScale(float(scale))
                , mMalleable(definition.mMalleable)
                , mTau(definition.mTau)
                , mDamping(definition.mDamping)
            {
                mRows = coordinates();
            }

            void getInfo1(btConstraintInfo1* info) override
            {
                mRows = coordinates();
                // Three bilateral anchor rows, plus a lower and upper
                // unilateral velocity bound for each native angular coordinate.
                info->m_numConstraintRows = 3 + 2 * int(mRows.size()) + (mJoint.mFriction > 0 ? 3 : 0);
                info->nub = 3;
            }

            void getInfo2(btConstraintInfo2* info) override
            {
                if (mMalleable)
                    info->erp = mTau;
                btPoint2PointConstraint::getInfo2(info);
                int index = 3;
                for (const auto& row : mRows)
                {
                    for (bool lower : {true, false})
                    {
                        const int offset = index++ * info->rowskip;
                        for (int axis = 0; axis < 3; ++axis)
                        {
                            info->m_J1angularAxis[offset + axis] = row.mAxis[axis];
                            info->m_J2angularAxis[offset + axis] = -row.mAxis[axis];
                        }
                        const btScalar distance = (lower ? row.mMin : row.mMax) - row.mAngle;
                        const bool violated = lower ? distance > 0 : distance < 0;
                        // Permit travel to an unviolated boundary during this
                        // step. Existing penetration uses Bullet's error
                        // reduction, as do the anchor rows above.
                        info->m_constraintError[offset] = info->fps * distance * (violated ? info->erp : 1);
                        info->m_lowerLimit[offset] = lower ? 0 : -SIMD_INFINITY;
                        info->m_upperLimit[offset] = lower ? SIMD_INFINITY : 0;
                    }
                }
                if (mJoint.mFriction > 0)
                {
                    const auto basis = m_rbA.getCenterOfMassTransform().getBasis() * mShapeToCenterA;
                    const auto maximum = ragdollFrictionImpulse(mJoint.mFriction, float(1 / info->fps), mLengthScale);
                    for (int axis = 0; axis < 3; ++axis)
                        frictionRow(info, index++, basis.getColumn(axis), maximum);
                }
                if (mMalleable)
                    info->m_damping = mDamping;
            }
        };
    }

    struct ActorRagdollPhysics::Impl
    {
        struct Body
        {
            std::uint32_t mRecord;
            btTransform mCenterFrame;
            float mLinearDamping, mAngularDamping;
            RagdollMotionLimits mLimits;
            std::unique_ptr<btRigidBody> mBody;
            std::size_t mActivationGroup = 0;
            btScalar mDynamicMass = 0;
            btVector3 mDynamicInertia{0, 0, 0};
            RagdollNativeMotion mMotion = RagdollNativeMotion::Dynamic;
        };
        btDynamicsWorld& mWorld;
        float mLengthScale;
        // Bodies are destroyed before shapes; neither owns the other's storage.
        std::vector<std::unique_ptr<btCollisionShape>> mShapes;
        std::vector<Body> mBodies;
        std::vector<std::vector<std::size_t>> mActivationGroups;
        std::vector<btCollisionObject*> mCollisionObjects;
        std::vector<std::unique_ptr<btTypedConstraint>> mConstraints;
        std::size_t mRegisteredBodies = 0, mRegisteredConstraints = 0;

        void setMotion(Body& owned, RagdollNativeMotion motion)
        {
            auto& body = *owned.mBody;
            const int flags = body.getCollisionFlags()
                & ~(btCollisionObject::CF_STATIC_OBJECT | btCollisionObject::CF_KINEMATIC_OBJECT);
            if (motion == RagdollNativeMotion::Keyframed)
            {
                body.setMassProps(0, btVector3(0, 0, 0));
                body.setCollisionFlags(flags | btCollisionObject::CF_KINEMATIC_OBJECT);
                body.forceActivationState(DISABLE_DEACTIVATION);
            }
            else
            {
                body.setMassProps(owned.mDynamicMass, owned.mDynamicInertia);
                body.setCollisionFlags(flags);
                body.forceActivationState(ACTIVE_TAG);
            }
            body.updateInertiaTensor();
            owned.mMotion = motion;
        }

        void activateGroup(std::size_t group)
        {
            for (const auto index : mActivationGroups[group])
                mBodies[index].mBody->activate(true);
        }

        Impl(btDynamicsWorld& world, float scale) : mWorld(world), mLengthScale(scale) {}
        ~Impl()
        {
            while (mRegisteredConstraints)
                mWorld.removeConstraint(mConstraints[--mRegisteredConstraints].get());
            while (mRegisteredBodies)
                mWorld.removeRigidBody(mBodies[--mRegisteredBodies].mBody.get());
        }
    };

    osg::Vec3f ragdollWorldToNativePosition(const osg::Vec3f& position)
    {
        osg::Vec3f result;
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            require(std::isfinite(position[axis]), "nonfinite world position");
            result[axis] = float(double(position[axis]) * 0.1428767293691635);
            require(std::isfinite(result[axis]), "position exceeds native float domain");
        }
        return result;
    }

    osg::Vec3f ragdollNativeToWorldPosition(const osg::Vec3f& position)
    {
        osg::Vec3f result;
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            require(std::isfinite(position[axis]), "nonfinite native position");
            result[axis] = float(double(position[axis]) * double(RagdollNativeLengthScale));
            require(std::isfinite(result[axis]), "position exceeds world float domain");
        }
        return result;
    }

    RagdollNativeTargetPose ragdollNativeSceneTargetPose(const osg::Matrixf& worldPose)
    {
        for (unsigned i = 0; i < 16; ++i)
            require(std::isfinite(worldPose.ptr()[i]), "nonfinite bone world pose");
        for (unsigned row = 0; row < 3; ++row)
            require(worldPose(row, 3) == 0, "projective bone world pose");
        require(worldPose(3, 3) == 1, "invalid affine bone world pose");
        // OSG row-vector storage is the transpose of the native NiMatrix3.
        const btMatrix3x3 basis(worldPose(0, 0), worldPose(1, 0), worldPose(2, 0),
            worldPose(0, 1), worldPose(1, 1), worldPose(2, 1),
            worldPose(0, 2), worldPose(1, 2), worldPose(2, 2));
        validatePose(btTransform(basis, btVector3(0, 0, 0)));
        std::array<float, 9> matrix;
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned col = 0; col < 3; ++col)
                matrix[row * 3 + col] = worldPose(col, row);
        RagdollNativeTargetPose result;
        result.mPosition = ragdollWorldToNativePosition(worldPose.getTrans());
        auto& q = result.mRotation;
        // Actual7150F0 stores the trace, square-root input/result and reciprocal
        // factor as binary32; differences/products retain double intermediates.
        const float trace = float(double(matrix[0]) + matrix[4] + matrix[8]);
        if (trace > 0.f)
        {
            const float root = float(std::sqrt(double(float(double(trace) + 1.0))));
            q[3] = float(double(root) * .5);
            const float factor = float(.5 / double(root));
            q[0] = float((double(matrix[7]) - matrix[5]) * factor);
            q[1] = float((double(matrix[2]) - matrix[6]) * factor);
            q[2] = float((double(matrix[3]) - matrix[1]) * factor);
        }
        else
        {
            unsigned axis = matrix[4] > matrix[0] ? 1 : 0;
            if (matrix[8] > matrix[axis * 3 + axis])
                axis = 2;
            const unsigned next = (axis + 1) % 3;
            const unsigned last = (next + 1) % 3;
            const float squared = float(double(matrix[axis * 3 + axis])
                - matrix[next * 3 + next] - matrix[last * 3 + last] + 1.0);
            const float root = float(std::sqrt(double(squared)));
            q[axis] = float(double(root) * .5);
            const float factor = float(.5 / double(root));
            q[3] = float((double(matrix[last * 3 + next]) - matrix[next * 3 + last]) * factor);
            q[next] = float((double(matrix[axis * 3 + next]) + matrix[next * 3 + axis]) * factor);
            q[last] = float((double(matrix[axis * 3 + last]) + matrix[last * 3 + axis]) * factor);
        }
        // Scene sync reorders WXYZ to XYZW, then executes4D6830 once.
        // Match its float reduction/Newton stores with portable reciprocal sqrt.
        const float xx = q[0] * q[0];
        const float yy = q[1] * q[1];
        const float zz = q[2] * q[2];
        const float ww = q[3] * q[3];
        const float xxzz = zz + xx;
        const float yyww = ww + yy;
        const float squared = yyww + xxzz;
        require(std::isfinite(squared) && squared > 0.f, "invalid native scene rotation");
        const float reciprocal = 1.f / std::sqrt(squared);
        const float first = squared * reciprocal;
        const float second = first * reciprocal;
        const float error = 3.f - second;
        const float half = .5f * reciprocal;
        const float factor = half * error;
        for (float& value : q)
            value *= factor;
        return result;
    }

    RagdollNativeTargetPose ragdollNativeBlendSceneTargetPose(const osg::Matrixf& worldPose)
    {
        for (unsigned i = 0; i < 16; ++i)
            require(std::isfinite(worldPose.ptr()[i]), "nonfinite bone world pose");
        for (unsigned row = 0; row < 3; ++row)
            require(worldPose(row, 3) == 0, "projective bone world pose");
        require(worldPose(3, 3) == 1, "invalid affine bone world pose");
        // OSG row-vector storage is the transpose of the native NiMatrix3.
        const btMatrix3x3 basis(worldPose(0, 0), worldPose(1, 0), worldPose(2, 0),
            worldPose(0, 1), worldPose(1, 1), worldPose(2, 1),
            worldPose(0, 2), worldPose(1, 2), worldPose(2, 2));
        validatePose(btTransform(basis, btVector3(0, 0, 0)));
        std::array<float, 9> matrix;
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned col = 0; col < 3; ++col)
                matrix[row * 3 + col] = worldPose(col, row);
        RagdollNativeTargetPose result;
        result.mPosition = ragdollWorldToNativePosition(worldPose.getTrans());
        auto& q = result.mRotation;
        // Original539850 only transposes/pads NiMatrix3 into the Havok matrix.
        // Original8B1B40 retains trace, root and reciprocal on x87 through
        // each output float store. Use the original CRT53-bit precision domain;
        // no early binary32 trace/root stores and no quaternion normalization.
        const double trace = double(matrix[4]) + matrix[0] + matrix[8];
        if (trace > 0.)
        {
            const double root = std::sqrt(trace + 1.);
            require(std::isfinite(root) && root > 0., "invalid native blend rotation root");
            const double factor = .5 / root;
            q[0] = float((double(matrix[7]) - matrix[5]) * factor);
            q[1] = float((double(matrix[2]) - matrix[6]) * factor);
            q[2] = float((double(matrix[3]) - matrix[1]) * factor);
            q[3] = float(root * .5);
        }
        else
        {
            unsigned axis = matrix[4] > matrix[0] ? 1 : 0;
            if (matrix[8] > matrix[axis * 3 + axis])
                axis = 2;
            const unsigned next = (axis + 1) % 3;
            const unsigned last = (next + 1) % 3;
            const double squared = double(matrix[axis * 3 + axis])
                - (double(matrix[next * 3 + next]) + matrix[last * 3 + last]) + 1.;
            const double root = std::sqrt(squared);
            require(std::isfinite(root) && root > 0., "invalid native blend rotation root");
            const double factor = .5 / root;
            q[axis] = float(root * .5);
            q[3] = float((double(matrix[last * 3 + next]) - matrix[next * 3 + last]) * factor);
            q[next] = float((double(matrix[axis * 3 + next]) + matrix[next * 3 + axis]) * factor);
            q[last] = float((double(matrix[axis * 3 + last]) + matrix[last * 3 + axis]) * factor);
        }
        return result;
    }

    namespace
    {
        void validateNativeOffsetRotation(const std::array<float, 4>& rotation)
        {
            double norm = 0;
            for (const float value : rotation)
            {
                require(std::isfinite(value), "nonfinite native offset quaternion");
                norm += double(value) * value;
            }
            require(std::abs(norm - 1.0) <= 1e-4, "nonunit native offset quaternion");
        }

        std::array<float, 4> bodyTLocalRotation(const RagdollBodyDefinition& body)
        {
            std::array<float, 4> local;
            for (unsigned i = 0; i < 4; ++i)
                local[i] = float(body.mRotation[i]);
            validateNativeOffsetRotation(local);
            for (unsigned i = 0; i < 3; ++i)
                require(std::isfinite(body.mTranslation[i]), "nonfinite bodyT local position");
            return local;
        }

        osg::Vec3f nativeBodyTOffset(const std::array<float, 4>& q, const osg::Vec3f& translation)
        {
            // Original8B9400/8B9150 rotate native-length local offset with
            // SSE binary32 products/sums and three x87 coefficient stores.
            const float dotX = q[0] * translation[0];
            const float dotY = q[1] * translation[1];
            const float dotZ = q[2] * translation[2];
            const float dotXY = dotY + dotX;
            const float dot = dotZ + dotXY;
            const float axisCoefficient = float(2.0 * dot);
            const float scalarCoefficient = float(2.0 * double(q[3]) * q[3] - 1.0);
            const float crossCoefficient = float(2.0 * q[3]);
            osg::Vec3f result;
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                const unsigned next = (axis + 1) % 3;
                const unsigned last = (axis + 2) % 3;
                const float first = q[next] * translation[last];
                const float second = q[last] * translation[next];
                const float cross = first - second;
                const float crossTerm = cross * crossCoefficient;
                const float scalarTerm = scalarCoefficient * translation[axis];
                const float axisTerm = axisCoefficient * q[axis];
                const float sum = scalarTerm + axisTerm;
                result[axis] = crossTerm + sum;
                require(std::isfinite(result[axis]), "nonfinite bodyT rotated offset");
            }
            return result;
        }
    }

    RagdollNativeTargetPose ragdollNativeSceneBodyTargetPose(
        const osg::Matrixf& worldPose, const RagdollBodyDefinition& body)
    {
        const auto parent = ragdollNativeSceneTargetPose(worldPose);
        if (!body.mUsesRigidBodyTransform)
            return parent;
        const auto local = bodyTLocalRotation(body);
        const auto& q = parent.mRotation;
        const auto offset = nativeBodyTOffset(q, body.mTranslation);
        RagdollNativeTargetPose result;
        std::array<float, 3> products;
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            result.mPosition[axis] = offset[axis] + parent.mPosition[axis];
            const unsigned next = (axis + 1) % 3;
            const unsigned last = (axis + 2) % 3;
            // Full889470: parent * local, with no additional normalization.
            const float firstProduct = local[last] * q[next];
            const float secondProduct = local[next] * q[last];
            const float quaternionCross = firstProduct - secondProduct;
            const float parentTerm = q[3] * local[axis];
            const float localTerm = local[3] * q[axis];
            const float quaternionSum = parentTerm + quaternionCross;
            result.mRotation[axis] = localTerm + quaternionSum;
            products[axis] = local[axis] * q[axis];
            require(std::isfinite(result.mPosition[axis]) && std::isfinite(result.mRotation[axis]),
                "nonfinite bodyT scene target");
        }
        const float productXY = products[1] + products[0];
        const float productXYZ = products[2] + productXY;
        result.mRotation[3] = float(double(q[3]) * local[3] - productXYZ);
        require(std::isfinite(result.mRotation[3]), "nonfinite bodyT scene quaternion");
        return result;
    }

    RagdollNativeTargetPose ragdollNativeSceneTargetFromBodyPose(
        const RagdollNativeTargetPose& bodyPose, const RagdollBodyDefinition& body)
    {
        validateNativeOffsetRotation(bodyPose.mRotation);
        for (unsigned axis = 0; axis < 3; ++axis)
            require(std::isfinite(bodyPose.mPosition[axis]), "nonfinite native body origin");
        if (!body.mUsesRigidBodyTransform)
            return bodyPose;
        const auto local = bodyTLocalRotation(body);
        const auto& q = bodyPose.mRotation;
        RagdollNativeTargetPose result;
        std::array<float, 4> products;
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            // Full8A2B40 computes body * conjugate(local), with a distinct
            // SSE scalar dot reduction rather than forward889470's x87 W.
            const unsigned next = (axis + 1) % 3;
            const unsigned last = (axis + 2) % 3;
            const float firstProduct = local[next] * q[last];
            const float secondProduct = local[last] * q[next];
            const float cross = firstProduct - secondProduct;
            const float parentTerm = q[3] * local[axis];
            const float difference = cross - parentTerm;
            const float localTerm = local[3] * q[axis];
            result.mRotation[axis] = localTerm + difference;
        }
        for (unsigned axis = 0; axis < 4; ++axis)
            products[axis] = local[axis] * q[axis];
        const float productXZ = products[2] + products[0];
        const float productYW = products[3] + products[1];
        result.mRotation[3] = productYW + productXZ;
        for (const float value : result.mRotation)
            require(std::isfinite(value), "nonfinite bodyT reverse quaternion");
        const auto offset = nativeBodyTOffset(result.mRotation, body.mTranslation);
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            result.mPosition[axis] = bodyPose.mPosition[axis] - offset[axis];
            require(std::isfinite(result.mPosition[axis]), "nonfinite bodyT reverse position");
        }
        return result;
    }

    btTransform ragdollNativePoseFromBoneWorld(const osg::Matrixf& worldPose)
    {
        const auto target = ragdollNativeSceneTargetPose(worldPose);
        return btTransform(btQuaternion(target.mRotation[0], target.mRotation[1],
            target.mRotation[2], target.mRotation[3]), vector(target.mPosition));
    }

    namespace
    {
        osg::Matrixf nativeSceneMatrix(const RagdollNativeTargetPose& pose, bool mixed)
        {
            double norm = 0;
            for (const auto value : pose.mRotation)
            {
                require(std::isfinite(value), "nonfinite native scene quaternion");
                norm += double(value) * double(value);
            }
            require(std::abs(norm - 1.) <= 1e-4, "nonunit native scene quaternion");
            const auto position = ragdollNativeToWorldPosition(
                osg::Vec3f(pose.mPosition[0], pose.mPosition[1], pose.mPosition[2]));
            const auto& q = pose.mRotation;
            // Original47C600 stores all nine products. Original8B1DD0 keeps
            // WY and WZ at x87 precision until the final matrix stores. Both
            // subtract the summed diagonal products, without renormalizing.
            const float tx = float(double(q[0]) * 2.);
            const float ty = float(double(q[1]) * 2.);
            const float tz = float(double(q[2]) * 2.);
            const float wx = float(double(q[3]) * double(tx));
            const double wy = mixed ? double(q[3]) * double(ty) : float(double(q[3]) * double(ty));
            const double wz = mixed ? double(q[3]) * double(tz) : float(double(q[3]) * double(tz));
            const float xx = float(double(q[0]) * double(tx));
            const float xy = float(double(q[0]) * double(ty));
            const float xz = float(double(q[0]) * double(tz));
            const float yy = float(double(q[1]) * double(ty));
            const float yz = float(double(q[1]) * double(tz));
            const float zz = float(double(q[2]) * double(tz));
            // OSG row vectors transpose the native NiMatrix3 column convention.
            osg::Matrixf result;
            result(0, 0) = float(1. - (double(yy) + double(zz)));
            result(0, 1) = float(double(xy) + double(wz));
            result(0, 2) = float(double(xz) - double(wy));
            result(1, 0) = float(double(xy) - double(wz));
            result(1, 1) = float(1. - (double(xx) + double(zz)));
            result(1, 2) = float(double(yz) + double(wx));
            result(2, 0) = float(double(xz) + double(wy));
            result(2, 1) = float(double(yz) - double(wx));
            result(2, 2) = float(1. - (double(xx) + double(yy)));
            result.setTrans(position);
            return result;
        }

    }

    osg::Matrixf ragdollBoneWorldFromNativePose(const RagdollNativeTargetPose& pose)
    {
        return nativeSceneMatrix(pose, false);
    }

    osg::Matrixf ragdollBoneWorldFromNativeBlendPose(const RagdollNativeTargetPose& pose)
    {
        return nativeSceneMatrix(pose, true);
    }

    RagdollNativeBlendPoseTargets ragdollNativeBlendPoseTargets(const RagdollNativeTargetPose& physical,
        const osg::Matrixf& animatedWorld, float hierarchyGain, std::uint16_t collisionFlags)
    {
        const auto animated = ragdollNativeBlendSceneTargetPose(animatedWorld);
        const auto drive = ragdollNativeBlendTargetPose(physical, animated, hierarchyGain);
        auto scene = drive;
        if (!(collisionFlags & 0x100))
            scene.mPosition = animated.mPosition;
        return {drive, ragdollBoneWorldFromNativeBlendPose(scene)};
    }

    std::vector<btTransform> ragdollBodyWorldPoses(const ActorRagdollDefinition& definition,
        std::span<const RagdollBoneWorldPose> bones)
    {
        require(!definition.mBodies.empty() && bones.size() == definition.mBodies.size(), "bone pose count");
        std::unordered_map<std::uint32_t, const osg::Matrixf*> poses;
        for (const auto& bone : bones)
            require(poses.emplace(bone.mNodeRecord, &bone.mPose).second, "duplicate posed bone identity");
        std::unordered_set<std::uint32_t> records, nodes;
        std::vector<btTransform> result;
        result.reserve(definition.mBodies.size());
        for (const auto& body : definition.mBodies)
        {
            require(records.insert(body.mRecord).second && nodes.insert(body.mNodeRecord).second,
                "duplicate body/bone identity");
            require(!body.mUsesRigidBodyTransform, "unadmitted bhkRigidBodyT pose binding");
            const auto found = poses.find(body.mNodeRecord);
            require(found != poses.end(), "missing current bone world pose");
            auto pose = ragdollNativePoseFromBoneWorld(*found->second);
            pose.setOrigin(pose.getOrigin() * btScalar(RagdollNativeLengthScale));
            result.push_back(pose);
        }
        return result;
    }

    btScalar ragdollFrictionImpulse(float torque, float frameSeconds, float lengthScale)
    {
        coefficient(torque);
        coefficient(frameSeconds);
        require(std::isfinite(lengthScale) && lengthScale > 0, "invalid friction length scale");
        const float native = float(double(torque) * double(frameSeconds));
        require(std::isfinite(native), "nonfinite friction impulse");
        const btScalar result = btScalar(native) * btScalar(lengthScale) * btScalar(lengthScale);
        require(std::isfinite(result), "nonfinite scaled friction impulse");
        return result;
    }

    ActorRagdollPhysics::ActorRagdollPhysics(const ActorRagdollDefinition& definition, btDynamicsWorld& world,
        float lengthScale, std::span<const btTransform> bodyPoses, int collisionGroup, int collisionMask, void* userPointer,
        const RagdollInternalCollisionFilter* internalFilter)
        : mImpl(std::make_unique<Impl>(world, lengthScale))
    {
        require(std::isfinite(lengthScale) && lengthScale > 0, "invalid length scale");
        require(!definition.mBodies.empty() && bodyPoses.size() == definition.mBodies.size(), "pose count");
        std::unordered_set<std::uint32_t> records;
        for (std::size_t i = 0; i < definition.mBodies.size(); ++i)
        {
            const auto& input = definition.mBodies[i];
            require(records.insert(input.mRecord).second, "duplicate body identity");
            validatePose(bodyPoses[i]);
            require(std::isfinite(input.mMass) && input.mMass > 0, "invalid mass");
            coefficient(input.mLinearDamping);
            coefficient(input.mAngularDamping);
            const auto limits = ragdollLoadedMotionLimits(input.mLinearDamping, input.mAngularDamping,
                input.mMaxLinearVelocity, input.mMaxAngularVelocity);
            coefficient(input.mFriction);
            coefficient(input.mRestitution);
            const btScalar scale = lengthScale;
            auto shape = std::visit([scale](const auto& value) -> std::unique_ptr<btCollisionShape> {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, RagdollSphere>)
                {
                    radius(value.mRadius);
                    return std::make_unique<btSphereShape>(value.mRadius * scale);
                }
                else if constexpr (std::is_same_v<T, RagdollCapsule>)
                {
                    radius(value.mRadius1);
                    radius(value.mRadius2);
                    const btVector3 points[] = {vector(value.mPoint1) * scale, vector(value.mPoint2) * scale};
                    const btScalar radii[] = {value.mRadius1 * scale, value.mRadius2 * scale};
                    return std::make_unique<btMultiSphereShape>(points, radii, 2);
                }
                else
                {
                    coefficient(value.mRadius);
                    require(value.mVertices.size() >= 4, "insufficient hull vertices");
                    auto result = std::make_unique<btConvexHullShape>();
                    for (const auto& point : value.mVertices)
                        result->addPoint(vector(point) * scale, false);
                    result->setMargin(value.mRadius * scale);
                    result->recalcLocalAabb();
                    return result;
                }
            }, input.mShape);
            btMatrix3x3 inertia;
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 3; ++col)
                {
                    const float value = input.mInertia[row * 3 + col];
                    require(std::isfinite(value), "nonfinite inertia");
                    require(value == input.mInertia[col * 3 + row], "asymmetric inertia");
                    inertia[row][col] = btScalar(value) * scale * scale;
                }
            btMatrix3x3 principal;
            inertia.diagonalize(principal, btScalar(1e-12), 32);
            const btVector3 diagonal(inertia[0][0], inertia[1][1], inertia[2][2]);
            require(finite(diagonal) && diagonal.x() > 0 && diagonal.y() > 0 && diagonal.z() > 0,
                "nonpositive inertia");
            const btTransform centerFrame(principal, vector(input.mCenter) * scale);
            auto compound = std::make_unique<btCompoundShape>();
            compound->addChildShape(centerFrame.inverse(), shape.get());
            btRigidBody::btRigidBodyConstructionInfo info(input.mMass, nullptr, compound.get(), diagonal);
            info.m_startWorldTransform = bodyPoses[i] * centerFrame;
            info.m_friction = input.mFriction;
            info.m_restitution = input.mRestitution;
            // Havok coefficients above one cannot be passed to Bullet's
            // exponential damping API. The serial caller applies native damping.
            info.m_linearDamping = info.m_angularDamping = 0;
            auto body = std::make_unique<btRigidBody>(info);
            body->setUserPointer(userPointer);
            mImpl->mCollisionObjects.push_back(body.get());
            mImpl->mShapes.push_back(std::move(shape));
            mImpl->mShapes.push_back(std::move(compound));
            mImpl->mBodies.push_back({input.mRecord, centerFrame, input.mLinearDamping,
                input.mAngularDamping, limits, std::move(body)});
            mImpl->mBodies.back().mDynamicMass = input.mMass;
            mImpl->mBodies.back().mDynamicInertia = diagonal;
        }
        for (const auto& input : definition.mJoints)
        {
            require(input.mBodyA < mImpl->mBodies.size() && input.mBodyB < mImpl->mBodies.size()
                && input.mBodyA != input.mBodyB, "invalid joint endpoints");
            if (input.mMalleable)
            {
                coefficient(input.mTau);
                coefficient(input.mDamping);
            }
            auto& a = mImpl->mBodies[input.mBodyA];
            auto& b = mImpl->mBodies[input.mBodyB];
            if (const auto* cone = std::get_if<RagdollConeJoint>(&input.mJoint))
            {
                coefficient(cone->mFriction);
                mImpl->mConstraints.push_back(std::make_unique<NativeConeConstraint>(*a.mBody, *b.mBody,
                    *cone, a.mCenterFrame, b.mCenterFrame, lengthScale, input));
                continue;
            }
            const auto* hinge = std::get_if<RagdollHingeJoint>(&input.mJoint);
            coefficient(hinge->mFriction);
            require(std::isfinite(hinge->mMin) && std::isfinite(hinge->mMax)
                && hinge->mMin <= hinge->mMax, "invalid hinge limits");
            auto constraint = std::make_unique<NativeHingeConstraint>(*a.mBody, *b.mBody,
                a.mCenterFrame.inverse() * hingeFrame(hinge->mA, lengthScale),
                b.mCenterFrame.inverse() * hingeFrame(hinge->mB, lengthScale), hinge->mFriction, lengthScale, input);
            constraint->setLimit(hinge->mMin, hinge->mMax);
            mImpl->mConstraints.push_back(std::move(constraint));
        }
        if (internalFilter)
        {
            std::vector<std::uint32_t> filters;
            filters.reserve(definition.mBodies.size());
            for (const auto& body : definition.mBodies)
            {
                const auto& fields = body.mInfoFilter;
                const auto value = std::uint32_t(fields.mLayer) | (std::uint32_t(fields.mFlags) << 8)
                    | (std::uint32_t(internalFilter->mSystemGroup) << 16);
                // Validate even a graph with one body or a wildcard group.
                internalFilter->mMasks.enabled(value, value);
                filters.push_back(value);
            }
            for (std::size_t a = 0; a < filters.size(); ++a)
                for (std::size_t b = a + 1; b < filters.size(); ++b)
                {
                    const bool forward = internalFilter->mMasks.enabled(filters[a], filters[b]);
                    const bool backward = internalFilter->mMasks.enabled(filters[b], filters[a]);
                    require(forward == backward, "asymmetric internal collision filter");
                    if (!forward)
                    {
                        mImpl->mBodies[a].mBody->setIgnoreCollisionCheck(mImpl->mBodies[b].mBody.get(), true);
                        mImpl->mBodies[b].mBody->setIgnoreCollisionCheck(mImpl->mBodies[a].mBody.get(), true);
                    }
                }
        }
        // Native activation belongs to an island, not just the setter's body.
        // Reconstruct constraint-connected owned groups once so every connected
        // bone enters our native gravity/damping pass before Bullet's island step.
        // Contact connections outside this owned graph remain world authority.
        std::vector<std::size_t> parent(mImpl->mBodies.size());
        std::iota(parent.begin(), parent.end(), 0);
        const auto root = [&](std::size_t index) {
            while (parent[index] != index)
            {
                parent[index] = parent[parent[index]];
                index = parent[index];
            }
            return index;
        };
        for (const auto& joint : definition.mJoints)
            parent[root(joint.mBodyB)] = root(joint.mBodyA);
        mImpl->mActivationGroups.resize(mImpl->mBodies.size());
        for (std::size_t index = 0; index < mImpl->mBodies.size(); ++index)
        {
            const auto group = root(index);
            mImpl->mBodies[index].mActivationGroup = group;
            mImpl->mActivationGroups[group].push_back(index);
        }
        // Publish only after every body, constraint and filter was admitted.
        for (auto& body : mImpl->mBodies)
        {
            world.addRigidBody(body.mBody.get(), collisionGroup, collisionMask);
            ++mImpl->mRegisteredBodies;
        }
        for (auto& constraint : mImpl->mConstraints)
        {
            world.addConstraint(constraint.get(), internalFilter == nullptr);
            ++mImpl->mRegisteredConstraints;
        }
    }

    ActorRagdollPhysics::~ActorRagdollPhysics() = default;

    std::span<btCollisionObject* const> ActorRagdollPhysics::collisionObjects() const
    {
        return mImpl->mCollisionObjects;
    }

    std::vector<RagdollBodyState> ActorRagdollPhysics::capture() const
    {
        std::vector<RagdollBodyState> result;
        for (const auto& body : mImpl->mBodies)
            result.push_back({body.mRecord, body.mBody->getWorldTransform() * body.mCenterFrame.inverse(),
                body.mBody->getLinearVelocity(), body.mBody->getAngularVelocity()});
        return result;
    }

    void ActorRagdollPhysics::restore(std::span<const RagdollBodyState> states)
    {
        require(states.size() == mImpl->mBodies.size(), "state count");
        for (std::size_t i = 0; i < states.size(); ++i)
        {
            require(states[i].mRecord == mImpl->mBodies[i].mRecord, "state identity");
            validatePose(states[i].mPose);
            require(finite(states[i].mLinearVelocity) && finite(states[i].mAngularVelocity), "nonfinite velocity");
        }
        for (std::size_t i = 0; i < states.size(); ++i)
        {
            auto& body = *mImpl->mBodies[i].mBody;
            const auto pose = states[i].mPose * mImpl->mBodies[i].mCenterFrame;
            body.setLinearVelocity(states[i].mLinearVelocity);
            body.setAngularVelocity(states[i].mAngularVelocity);
            // Refresh the rotated world inertia and copy the restored velocities
            // into interpolation state as well as the current transform.
            body.setCenterOfMassTransform(pose);
            body.clearForces();
            body.activate(true);
            mImpl->mWorld.updateSingleAabb(&body);
        }
    }

    std::vector<RagdollNativeMotionRequest> ActorRagdollPhysics::captureNativeMotionModes() const
    {
        std::vector<RagdollNativeMotionRequest> result;
        for (const auto& body : mImpl->mBodies)
            result.push_back({body.mRecord, body.mMotion});
        return result;
    }

    void ActorRagdollPhysics::setNativeMotionModes(std::span<const RagdollNativeMotionRequest> requests)
    {
        struct Pending
        {
            Impl::Body* mOwned;
            RagdollNativeMotion mMotion;
        };
        std::vector<Pending> pending;
        pending.reserve(requests.size());
        std::unordered_set<std::uint32_t> selected;
        for (const auto& request : requests)
        {
            require(request.mMotion == RagdollNativeMotion::Dynamic
                || request.mMotion == RagdollNativeMotion::Keyframed, "invalid native body motion request");
            require(selected.insert(request.mRecord).second, "duplicate native body motion identity");
            const auto found = std::find_if(mImpl->mBodies.begin(), mImpl->mBodies.end(),
                [&](const auto& body) { return body.mRecord == request.mRecord; });
            require(found != mImpl->mBodies.end(), "unknown native body motion identity");
            if (found->mMotion != request.mMotion)
                pending.push_back({&*found, request.mMotion});
        }
        // Original8CBC60 archives the dynamic motion's physical properties,
        // then copies current motion state and velocities on both handoffs.
        // Keep the same Bullet object/shape/constraints and preserve those
        // fields directly; do not reconstruct them from a stale pose snapshot.
        for (const auto& change : pending)
        {
            auto& owned = *change.mOwned;
            mImpl->setMotion(owned, change.mMotion);
        }
        for (const auto& change : pending)
            if (change.mMotion == RagdollNativeMotion::Dynamic)
                mImpl->activateGroup(change.mOwned->mActivationGroup);
    }

    void ActorRagdollPhysics::synchronizeNativeKeyframedPoses(
        std::span<const RagdollNativeScenePoseRequest> poses)
    {
        struct Pending
        {
            Impl::Body* mOwned;
            btTransform mPose;
        };
        std::vector<Pending> pending;
        pending.reserve(poses.size());
        std::unordered_set<std::uint32_t> selected;
        for (const auto& request : poses)
        {
            require(selected.insert(request.mRecord).second, "duplicate native scene pose identity");
            const auto found = std::find_if(mImpl->mBodies.begin(), mImpl->mBodies.end(),
                [&](const auto& body) { return body.mRecord == request.mRecord; });
            require(found != mImpl->mBodies.end(), "unknown native scene pose identity");
            require(found->mMotion == RagdollNativeMotion::Keyframed,
                "native scene pose publication requires keyframed motion");
            auto pose = ragdollNativePoseFromBoneWorld(request.mWorldPose);
            pose.setOrigin(pose.getOrigin() * btScalar(mImpl->mLengthScale));
            pose *= found->mCenterFrame;
            validatePose(pose);
            pending.push_back({&*found, pose});
        }
        for (const auto& change : pending)
            mImpl->activateGroup(change.mOwned->mActivationGroup);
        for (const auto& change : pending)
        {
            auto& body = *change.mOwned->mBody;
            // Original8DD970 replaces current and previous COM/rotation while
            // retaining motion velocities. Bullet's kinematic setter initially
            // keeps the old interpolation pose, so replace that explicitly.
            body.setCenterOfMassTransform(change.mPose);
            body.setInterpolationWorldTransform(change.mPose);
            mImpl->mWorld.updateSingleAabb(&body);
        }
    }

    std::vector<RagdollNativeBlendPublication> ActorRagdollPhysics::updateNativeBlends(
        std::span<const RagdollNativeBlendUpdate> updates, float preparedFrameSeconds,
        std::uint32_t rawUpdateSelector, float nativeGravityZ)
    {
        require(std::isfinite(nativeGravityZ), "invalid native blend gravity");
        ESM4::resolvePhysicalBlendDriveParameters(preparedFrameSeconds, 0, 0);
        struct Pending
        {
            Impl::Body* mOwned;
            RagdollNativeMotion mMotion;
            bool mChanged;
            std::optional<btTransform> mPose;
            std::optional<std::pair<btVector3, btVector3>> mVelocities;
        };
        std::vector<Pending> pending;
        std::vector<RagdollNativeBlendPublication> result;
        pending.reserve(updates.size());
        result.reserve(updates.size());
        std::unordered_set<std::uint32_t> selected;
        for (const auto& update : updates)
        {
            require(selected.insert(update.mRecord).second, "duplicate native blend body identity");
            const auto found = std::find_if(mImpl->mBodies.begin(), mImpl->mBodies.end(),
                [&](const auto& body) { return body.mRecord == update.mRecord; });
            require(found != mImpl->mBodies.end(), "unknown native blend body identity");
            const auto dispatch = ESM4::resolvePhysicalBlendDispatch(update.mHierarchyGain,
                update.mVelocityGain, update.mCollisionFlags, rawUpdateSelector);
            if (!dispatch)
                continue;
            const auto motion = dispatch->mMotion == ESM4::PhysicalBlendMotion::Keyframed
                ? RagdollNativeMotion::Keyframed : RagdollNativeMotion::Dynamic;
            const bool changed = found->mMotion != motion;
            Pending change{&*found, motion, changed, std::nullopt, std::nullopt};
            auto& body = *found->mBody;
            auto centerPose = body.getWorldTransform();
            auto shapePose = centerPose * found->mCenterFrame.inverse();
            RagdollNativeTargetPose physical;
            const auto rotation = shapePose.getRotation();
            for (unsigned axis = 0; axis < 4; ++axis)
                physical.mRotation[axis] = float(rotation[axis]);
            for (unsigned axis = 0; axis < 3; ++axis)
                physical.mPosition[axis] = float(shapePose.getOrigin()[axis] / mImpl->mLengthScale);

            // Leaving keyframed motion synchronizes scene before restoring the
            // dynamic archive. Entering keyframed motion zeroes velocities first.
            const bool sync = (changed && found->mMotion == RagdollNativeMotion::Keyframed)
                || dispatch->mRoute == ESM4::PhysicalBlendRoute::SceneToPhysics;
            if (sync)
            {
                // Flag0x20 without0x40 selects the distinct native World-driven
                // scene setter; this adapter has not admitted that path.
                require(!(update.mCollisionFlags & 0x20) || (update.mCollisionFlags & 0x40),
                    "unadmitted native World-driven scene synchronization");
                physical = ragdollNativeSceneTargetPose(update.mAnimatedWorld);
                shapePose = btTransform(btQuaternion(physical.mRotation[0], physical.mRotation[1],
                    physical.mRotation[2], physical.mRotation[3]), vector(physical.mPosition) * mImpl->mLengthScale);
                centerPose = shapePose * found->mCenterFrame;
                validatePose(centerPose);
                change.mPose = centerPose;
            }
            auto linear = body.getLinearVelocity();
            auto angular = body.getAngularVelocity();
            if (changed && found->mMotion == RagdollNativeMotion::Dynamic)
            {
                linear.setZero();
                angular.setZero();
                change.mVelocities = std::pair{linear, angular};
            }
            auto flags = update.mCollisionFlags;
            if (changed)
                flags = motion == RagdollNativeMotion::Keyframed ? flags & ~0x8 : flags | 0x8;
            RagdollNativeBlendPublication publication{update.mRecord, flags, std::nullopt};
            if (dispatch->mRoute == ESM4::PhysicalBlendRoute::PhysicsToScene)
                publication.mSceneTarget = ragdollBoneWorldFromNativePose(physical);
            else if (dispatch->mRoute == ESM4::PhysicalBlendRoute::PoseAndVelocity)
            {
                const auto targets = ragdollNativeBlendPoseTargets(physical, update.mAnimatedWorld,
                    update.mHierarchyGain, flags);
                if (rawUpdateSelector == 0)
                    publication.mSceneTarget = targets.mSceneTarget;
                const auto parameters = ESM4::resolvePhysicalBlendDriveParameters(
                    preparedFrameSeconds, update.mVelocityGain, flags);
                osg::Vec3f localCenter, currentCenter;
                RagdollNativeVelocities current;
                for (unsigned axis = 0; axis < 3; ++axis)
                {
                    localCenter[axis] = float(found->mCenterFrame.getOrigin()[axis] / mImpl->mLengthScale);
                    currentCenter[axis] = float(centerPose.getOrigin()[axis] / mImpl->mLengthScale);
                    current.mLinear[axis] = float(linear[axis] / mImpl->mLengthScale);
                    current.mAngular[axis] = float(angular[axis]);
                }
                const auto target = ragdollNativeTargetVelocities(localCenter, currentCenter,
                    physical.mRotation, targets.mDriveTarget, parameters.mInverseFrameSeconds,
                    found->mLimits.mMaxLinearVelocity, found->mLimits.mAngularLimit);
                const auto velocities = ragdollNativeBlendVelocities(current, target,
                    parameters.mVelocityGain, parameters.mInverseFrameSeconds, nativeGravityZ);
                for (unsigned axis = 0; axis < 3; ++axis)
                {
                    linear[axis] = btScalar(velocities.mLinear[axis]) * mImpl->mLengthScale;
                    angular[axis] = velocities.mAngular[axis];
                }
                require(finite(linear) && finite(angular), "native blend exceeds world velocity domain");
                change.mVelocities = std::pair{linear, angular};
            }
            pending.push_back(change);
            result.push_back(publication);
        }
        // All admission, allocation and native computations precede publication.
        for (const auto& change : pending)
        {
            auto& owned = *change.mOwned;
            auto& body = *owned.mBody;
            if (change.mChanged)
                mImpl->setMotion(owned, change.mMotion);
            if (change.mPose)
            {
                body.setCenterOfMassTransform(*change.mPose);
                body.setInterpolationWorldTransform(*change.mPose);
                mImpl->mWorld.updateSingleAabb(&body);
            }
            if (change.mVelocities)
            {
                body.setLinearVelocity(change.mVelocities->first);
                body.setAngularVelocity(change.mVelocities->second);
            }
            if (change.mChanged || change.mPose || change.mVelocities)
                mImpl->activateGroup(owned.mActivationGroup);
        }
        return result;
    }

    void ActorRagdollPhysics::applyImpulse(std::size_t body, const btVector3& impulse, const btVector3& worldPoint)
    {
        require(body < mImpl->mBodies.size() && finite(impulse) && finite(worldPoint), "invalid impulse");
        auto& target = *mImpl->mBodies[body].mBody;
        mImpl->activateGroup(mImpl->mBodies[body].mActivationGroup);
        target.applyImpulse(impulse, worldPoint - target.getCenterOfMassPosition());
    }

    void ActorRagdollPhysics::applyNativeVelocityStep(float frameSeconds,
        std::span<const osg::Vec3f> nativeLinearDeltas)
    {
        require(nativeLinearDeltas.size() == mImpl->mBodies.size(), "velocity delta count");
        auto states = capture();
        for (std::size_t i = 0; i < states.size(); ++i)
        {
            RagdollNativeVelocities input;
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                input.mLinear[axis] = float(states[i].mLinearVelocity[axis] / mImpl->mLengthScale);
                input.mAngular[axis] = float(states[i].mAngularVelocity[axis]);
            }
            const auto output = ragdollNativeVelocityStep(input, mImpl->mBodies[i].mLimits,
                frameSeconds, nativeLinearDeltas[i]);
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                states[i].mLinearVelocity[axis] = btScalar(output.mLinear[axis]) * mImpl->mLengthScale;
                states[i].mAngularVelocity[axis] = output.mAngular[axis];
            }
            require(finite(states[i].mLinearVelocity) && finite(states[i].mAngularVelocity),
                "velocity exceeds world domain");
        }
        for (std::size_t i = 0; i < states.size(); ++i)
        {
            // Native sleeping islands do not enter motion integration. Do not
            // accumulate gravity or wake a settled body without an impulse.
            if (mImpl->mBodies[i].mMotion == RagdollNativeMotion::Dynamic
                && mImpl->mBodies[i].mBody->isActive())
            {
                mImpl->mBodies[i].mBody->setLinearVelocity(states[i].mLinearVelocity);
                mImpl->mBodies[i].mBody->setAngularVelocity(states[i].mAngularVelocity);
            }
        }
    }

    void ActorRagdollPhysics::driveNativePoseVelocities(std::span<const RagdollNativeVelocityDrive> drives,
        float inverseFrameSeconds, float nativeGravityZ)
    {
        require(std::isfinite(inverseFrameSeconds) && inverseFrameSeconds > 0.f
            && std::isfinite(nativeGravityZ), "invalid native pose drive clock/gravity");
        struct Pending
        {
            btRigidBody* mBody;
            btVector3 mLinear, mAngular;
            std::size_t mActivationGroup;
        };
        std::vector<Pending> pending;
        pending.reserve(drives.size());
        std::unordered_set<std::uint32_t> selected;
        for (const auto& drive : drives)
        {
            require(selected.insert(drive.mRecord).second, "duplicate native pose drive identity");
            const auto found = std::find_if(mImpl->mBodies.begin(), mImpl->mBodies.end(),
                [&](const auto& body) { return body.mRecord == drive.mRecord; });
            require(found != mImpl->mBodies.end(), "unknown native pose drive body");
            require(found->mMotion == RagdollNativeMotion::Dynamic, "native pose drive requires dynamic motion");
            auto& body = *found->mBody;
            const auto shapePose = body.getWorldTransform() * found->mCenterFrame.inverse();
            const auto rotation = shapePose.getRotation();
            std::array<float, 4> quaternion;
            for (unsigned axis = 0; axis < 4; ++axis)
                quaternion[axis] = static_cast<float>(rotation[axis]);
            osg::Vec3f localCenter, currentCenter;
            RagdollNativeVelocities current;
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                // The owner's world/native length conversion is independent
                // of renderer adapters. Angular speed has no length factor.
                localCenter[axis] = static_cast<float>(found->mCenterFrame.getOrigin()[axis] / mImpl->mLengthScale);
                currentCenter[axis] = static_cast<float>(body.getCenterOfMassPosition()[axis] / mImpl->mLengthScale);
                current.mLinear[axis] = static_cast<float>(body.getLinearVelocity()[axis] / mImpl->mLengthScale);
                current.mAngular[axis] = static_cast<float>(body.getAngularVelocity()[axis]);
            }
            const auto target = ragdollNativeTargetVelocities(localCenter, currentCenter, quaternion,
                drive.mTarget, inverseFrameSeconds, found->mLimits.mMaxLinearVelocity, found->mLimits.mAngularLimit);
            const auto output = ragdollNativeBlendVelocities(current, target,
                drive.mVelocityGain, inverseFrameSeconds, nativeGravityZ);
            Pending result{&body, btVector3(0, 0, 0), btVector3(0, 0, 0), found->mActivationGroup};
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                result.mLinear[axis] = btScalar(output.mLinear[axis]) * mImpl->mLengthScale;
                result.mAngular[axis] = output.mAngular[axis];
            }
            require(finite(result.mLinear) && finite(result.mAngular), "pose drive exceeds world velocity domain");
            pending.push_back(result);
        }
        // Original8A6410 prepares activation before the actual89DB90/89DBB0
        // stores. Preserve live poses, forces, contacts and interpolation state.
        for (const auto& result : pending)
        {
            mImpl->activateGroup(result.mActivationGroup);
            result.mBody->setLinearVelocity(result.mLinear);
            result.mBody->setAngularVelocity(result.mAngular);
        }
    }

    void ActorRagdollPhysics::applyNativeDamping(float frameSeconds)
    {
        require(std::isfinite(frameSeconds) && frameSeconds >= 0, "invalid frame duration");
        auto states = capture();
        for (std::size_t i = 0; i < states.size(); ++i)
        {
            if (mImpl->mBodies[i].mMotion != RagdollNativeMotion::Dynamic)
                continue;
            const auto damp = [&](btVector3& velocity, float coefficient, btScalar scale) {
                const float factor = static_cast<float>(std::max(0.0, 1.0 - double(frameSeconds) * coefficient));
                for (int axis = 0; axis < 3; ++axis)
                {
                    const float native = static_cast<float>(velocity[axis] / scale);
                    require(std::isfinite(native), "velocity exceeds native float domain");
                    const float result = native * factor;
                    velocity[axis] = btScalar(result) * scale;
                }
            };
            damp(states[i].mLinearVelocity, mImpl->mBodies[i].mLinearDamping, mImpl->mLengthScale);
            // Angular velocity is radians/time, independent of length units.
            damp(states[i].mAngularVelocity, mImpl->mBodies[i].mAngularDamping, btScalar(1));
        }
        // Damping changes velocity only; retain the live contact/activation state.
        for (std::size_t i = 0; i < states.size(); ++i)
        {
            if (mImpl->mBodies[i].mMotion != RagdollNativeMotion::Dynamic)
                continue;
            mImpl->mBodies[i].mBody->setLinearVelocity(states[i].mLinearVelocity);
            mImpl->mBodies[i].mBody->setAngularVelocity(states[i].mAngularVelocity);
        }
    }
}
