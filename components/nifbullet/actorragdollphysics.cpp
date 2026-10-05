#include "actorragdollphysics.hpp"
#include "nativedynamicsworld.hpp"
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
            std::uint32_t mNodeRecord;
            btTransform mCenterFrame;
            float mLinearDamping, mAngularDamping;
            RagdollMotionLimits mLimits;
            std::unique_ptr<btRigidBody> mBody;
            std::size_t mActivationGroup = 0;
            btScalar mDynamicMass = 0;
            btVector3 mDynamicInertia{0, 0, 0};
            RagdollNativeMotion mMotion = RagdollNativeMotion::Dynamic;
            float mNativeLinearW = 0.f, mNativeAngularW = 0.f;
            float currentNativeLinearDamping() const
            {
                // Original8CBC60 creates KEY motion with zero current damping;
                // Dynamic restoration reuses the archived loaded coefficient.
                return mMotion == RagdollNativeMotion::Keyframed ? 0.f : mLinearDamping;
            }

            // Owned snapshot: only transform flag, local translation/rotation
            // are used. Shape properties remain in their existing owners.
            RagdollBodyDefinition mSceneOffset{};
        };
        struct BlendTarget
        {
            std::uint32_t mNode;
            RagdollNativeBlendState mState;
        };
        std::vector<RagdollNativeBlendControllerState> mBlendControllers;
        std::vector<RagdollNativeVelocityControllerState> mVelocityControllers;
        std::vector<BlendTarget> mBlendTargets;
        btDynamicsWorld& mWorld;
        float mLengthScale;
        // Bodies are destroyed before shapes; neither owns the other's storage.
        std::vector<std::unique_ptr<btCollisionShape>> mShapes;
        std::vector<Body> mBodies;
        std::vector<std::vector<std::size_t>> mActivationGroups;
        std::vector<btCollisionObject*> mCollisionObjects;
        std::vector<std::unique_ptr<btTypedConstraint>> mConstraints;
        std::size_t mRegisteredBodies = 0, mRegisteredConstraints = 0;
        int mCollisionGroup = 0, mCollisionMask = 0;
        bool mDisableLinkedCollisions = true;
        bool mPublished = false, mNativeWorld = false;
        std::weak_ptr<const NativeDynamicsWorld::Lifetime> mWorldLifetime;

        bool worldAlive() const noexcept
        {
            if (!mNativeWorld) return true;
            const auto lifetime = mWorldLifetime.lock();
            return lifetime && lifetime->mAlive;
        }
        void removePublished() noexcept
        {
            if (!worldAlive())
            {
                mRegisteredBodies = mRegisteredConstraints = 0;
                mPublished = false;
                return;
            }
            if (mPublished)
                if (auto* native = dynamic_cast<NativeDynamicsWorld*>(&mWorld))
                    native->unregisterNativeMotionOwner(mOwner);
            while (mRegisteredConstraints)
                mWorld.removeConstraint(mConstraints[--mRegisteredConstraints].get());
            while (mRegisteredBodies)
                mWorld.removeRigidBody(mBodies[--mRegisteredBodies].mBody.get());
            mPublished = false;
        }
        const void* mOwner = nullptr;

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
        ~Impl() { removePublished(); }
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

    osg::Vec3f ragdollNativeSceneCenterOfMass(const osg::Vec3f& bodyCenter,
        const std::array<float, 4>& bodyRotation, const RagdollBodyDefinition& body)
    {
        validateNativeOffsetRotation(bodyRotation);
        for (unsigned axis = 0; axis < 3; ++axis)
            require(std::isfinite(bodyCenter[axis]), "nonfinite native body center");
        if (!body.mUsesRigidBodyTransform)
            return bodyCenter;
        for (unsigned axis = 0; axis < 3; ++axis)
            require(std::isfinite(body.mTranslation[axis]), "nonfinite bodyT local position");
        // Native8B9050 uses motion+10 (8B1DD0 basis), not the reverse
        // scene quaternion or the quaternion-vector offset used by8B9150.
        const auto basis = ragdollBoneWorldFromNativeBlendPose({{}, bodyRotation});
        osg::Vec3f result;
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            // Complete88FE00: three SSE products, then (X+Y)+Z stores.
            const float x = basis(0, axis) * body.mTranslation[0];
            const float y = basis(1, axis) * body.mTranslation[1];
            const float z = basis(2, axis) * body.mTranslation[2];
            const float xy = x + y;
            const float offset = xy + z;
            result[axis] = bodyCenter[axis] - offset;
            require(std::isfinite(result[axis]), "nonfinite bodyT scene center");
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

    RagdollNativeKeyframedStepResult ragdollNativeKeyframedMotionStep(const osg::Vec3f& currentCenterOfMass,
        const std::array<float, 4>& currentRotation, const osg::Vec3f& localCenterOfMass,
        const RagdollNativeVelocities& velocities, float frameSeconds,
        float maximumLinearVelocity, float angularLimit, float linearW, float angularW)
    {
        const auto scalar = [](double value) {
            const float result = static_cast<float>(value);
            require(std::isfinite(result), "nonfinite keyframed motion result");
            return result;
        };
        const auto vectorFinite = [](const osg::Vec3f& value) {
            for (unsigned axis = 0; axis < 3; ++axis)
                require(std::isfinite(value[axis]), "nonfinite keyframed motion input");
        };
        const auto squaredLength = [&](const osg::Vec3f& value) {
            const float x = scalar(double(value[0]) * value[0]);
            const float y = scalar(double(value[1]) * value[1]);
            const float z = scalar(double(value[2]) * value[2]);
            const float xy = scalar(double(y) + x);
            return scalar(double(z) + xy);
        };
        const auto multiply = [&](osg::Vec3f& value, float factor) {
            for (unsigned axis = 0; axis < 3; ++axis)
                value[axis] = scalar(double(value[axis]) * factor);
        };
        for (float value : {frameSeconds, maximumLinearVelocity, angularLimit})
            require(std::isfinite(value) && value >= 0.f, "invalid keyframed motion coefficient");
        require(std::isfinite(linearW) && std::isfinite(angularW), "nonfinite keyframed packed velocity");
        vectorFinite(currentCenterOfMass);
        vectorFinite(localCenterOfMass);
        vectorFinite(velocities.mLinear);
        vectorFinite(velocities.mAngular);
        validateNativeOffsetRotation(currentRotation);
        RagdollNativeKeyframedStepResult result;
        result.mLinearW = linearW;
        result.mAngularW = angularW;
        result.mVelocities = velocities;
        // Keyframed motion has no gravity addition or damping multiplication.
        // Preserve untouched signed zeros, unlike the dynamic velocity step.
        const float linearSquared = squaredLength(result.mVelocities.mLinear);
        if (linearSquared > double(maximumLinearVelocity) * maximumLinearVelocity)
        {
            const float factor = scalar(double(maximumLinearVelocity) / std::sqrt(double(linearSquared)));
            multiply(result.mVelocities.mLinear, factor);
            result.mLinearW = scalar(double(result.mLinearW) * factor);
        }
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            const float increment = scalar(double(result.mVelocities.mLinear[axis]) * frameSeconds);
            result.mCenterOfMass[axis] = scalar(double(currentCenterOfMass[axis]) + increment);
        }
        auto angularStep = result.mVelocities.mAngular;
        multiply(angularStep, scalar(double(frameSeconds) * .5));
        float angularSquared = scalar(double(squaredLength(angularStep)) * 0.40528470277786255f);
        const double maximum = std::min(double(angularLimit) * frameSeconds, double(0.8999999761581421f));
        const float maximumSquared = scalar(maximum * maximum);
        if (angularSquared > maximumSquared)
        {
            const float factor = scalar(maximum / std::sqrt(double(angularSquared)));
            multiply(result.mVelocities.mAngular, factor);
            result.mAngularW = scalar(double(result.mAngularW) * factor);
            multiply(angularStep, factor);
            // Original8EA65E uses the stored cap square, not a recomputed
            // squared length of the rounded, capped half-angle vector.
            angularSquared = maximumSquared;
        }
        const double square = double(angularSquared) * angularSquared;
        const float w = scalar(((1.0 - double(angularSquared) * 0.8229479789733887f)
            - square * 0.1305290013551712f) - square * angularSquared * 0.04440800100564957f);
        // Full8EA6A4 passes incremental rotation first to889470: angular
        // velocity is in world space, so incremental * current is required.
        std::array<float, 4> q;
        std::array<float, 3> products;
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            const unsigned next = (axis + 1) % 3, last = (axis + 2) % 3;
            const float first = scalar(double(currentRotation[last]) * angularStep[next]);
            const float second = scalar(double(currentRotation[next]) * angularStep[last]);
            const float cross = scalar(double(first) - second);
            const float parent = scalar(double(w) * currentRotation[axis]);
            const float local = scalar(double(currentRotation[3]) * angularStep[axis]);
            const float sum = scalar(double(parent) + cross);
            q[axis] = scalar(double(local) + sum);
            products[axis] = scalar(double(angularStep[axis]) * currentRotation[axis]);
        }
        const float xy = scalar(double(products[1]) + products[0]);
        const float xyz = scalar(double(products[2]) + xy);
        q[3] = scalar(double(currentRotation[3]) * w - xyz);
        // Full4D6830 reduction/Newton stores with portable reciprocal sqrt.
        const float xx = scalar(double(q[0]) * q[0]), yy = scalar(double(q[1]) * q[1]);
        const float zz = scalar(double(q[2]) * q[2]), ww = scalar(double(q[3]) * q[3]);
        const float xz = scalar(double(zz) + xx), yw = scalar(double(ww) + yy);
        const float norm = scalar(double(yw) + xz);
        require(norm > 0.f, "zero keyframed motion rotation");
        const float reciprocal = 1.f / std::sqrt(norm);
        const float first = scalar(double(norm) * reciprocal);
        const float second = scalar(double(first) * reciprocal);
        const float error = scalar(3.0 - second);
        const float half = scalar(.5 * reciprocal);
        const float factor = scalar(double(half) * error);
        for (float& value : q)
            value = scalar(double(value) * factor);
        result.mBodyPose.mRotation = q;
        const auto basis = ragdollBoneWorldFromNativeBlendPose({{}, q});
        for (unsigned row = 0; row < 3; ++row)
        {
            // Full8EA713: physical basis times raw local COM; (X+Y)+Z stores.
            const float x = scalar(double(basis(0, row)) * localCenterOfMass[0]);
            const float y = scalar(double(basis(1, row)) * localCenterOfMass[1]);
            const float z = scalar(double(basis(2, row)) * localCenterOfMass[2]);
            const float originXY = scalar(double(x) + y);
            const float offset = scalar(double(originXY) + z);
            result.mBodyPose.mPosition[row] = scalar(double(result.mCenterOfMass[row]) - offset);
            result.mAngularDelta[row] = scalar(double(angularStep[row]) + angularStep[row]);
        }
        result.mAngularDelta[3] = scalar(std::sqrt(double(angularSquared)) * 3.1415927410125732f);
        return result;
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

    ActorRagdollDefinition ragdollDefinitionWithNativeLinkedBlendState(
        const ActorRagdollDefinition& authored, std::span<const std::uint32_t> resolvedPackedFilters,
        const ESM4::PhysicalBlendGainTable& resolvedGains)
    {
        require(resolvedPackedFilters.size() == authored.mBodies.size(), "linked blend filter count");
        auto linked = authored;
        for (std::size_t i = 0; i < linked.mBodies.size(); ++i)
        {
            auto& blend = linked.mBodies[i].mBlend;
            if (!blend)
                continue;
            const auto state = ESM4::resolvePhysicalBlendCollisionAfterLink(
                {blend->mFlags, {blend->mHierarchyGain, blend->mVelocityGain}, 8},
                true, resolvedPackedFilters[i], resolvedGains);
            blend->mFlags = state.mFlags;
            blend->mHierarchyGain = state.mGains.mHierarchy;
            blend->mVelocityGain = state.mGains.mVelocity;
        }
        return linked;
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
            const auto found = poses.find(body.mNodeRecord);
            require(found != poses.end(), "missing current bone world pose");
            const auto target = ragdollNativeSceneBodyTargetPose(*found->second, body);
            auto pose = btTransform(btQuaternion(target.mRotation[0], target.mRotation[1],
                target.mRotation[2], target.mRotation[3]), vector(target.mPosition));
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
        const RagdollInternalCollisionFilter* internalFilter, Publication publication)
        : mImpl(std::make_unique<Impl>(world, lengthScale))
    {
        auto* native = dynamic_cast<NativeDynamicsWorld*>(&world);
        require(publication == Publication::Immediate || publication == Publication::Deferred,
            "invalid graph publication mode");
        require(publication == Publication::Immediate || native,
            "deferred graph publication requires native World lifetime ownership");
        mImpl->mOwner = this;
        mImpl->mCollisionGroup = collisionGroup;
        mImpl->mCollisionMask = collisionMask;
        mImpl->mDisableLinkedCollisions = internalFilter == nullptr;
        mImpl->mNativeWorld = native != nullptr;
        if (native) mImpl->mWorldLifetime = native->lifetimeIdentity();
        require(std::isfinite(lengthScale) && lengthScale > 0, "invalid length scale");
        require(!definition.mBodies.empty() && bodyPoses.size() == definition.mBodies.size(), "pose count");
        const bool hasControllers = std::any_of(definition.mBodies.begin(), definition.mBodies.end(),
            [](const auto& body) { return body.mBlendController.has_value(); });
        std::unordered_set<std::uint32_t> nodes, controllers;
        if (hasControllers)
            for (const auto& body : definition.mBodies)
                require(nodes.insert(body.mNodeRecord).second, "ambiguous native controller target node");
        for (const auto& body : definition.mBodies)
        {
            if (body.mBlend)
            {
                require(std::isfinite(body.mBlend->mHierarchyGain) && std::isfinite(body.mBlend->mVelocityGain),
                    "nonfinite owned native blend gains");
                mImpl->mBlendTargets.push_back({body.mNodeRecord,
                    {body.mRecord, body.mBlend->mFlags, {body.mBlend->mHierarchyGain, body.mBlend->mVelocityGain}}});
            }
            if (!body.mBlendController)
                continue;
            const auto& source = *body.mBlendController;
            require(controllers.insert(source.mRecord).second, "duplicate owned native controller identity");
            require(!source.mTargetRecord || nodes.contains(*source.mTargetRecord),
                "unadmitted native controller target outside owned graph");
            ESM4::PhysicalBlendControllerState state;
            state.mTiming = {source.mFlags, source.mFrequency, source.mPhase, source.mStartTime, source.mStopTime};
            state.mKeys.reserve(source.mKeys.size());
            for (const auto& key : source.mKeys)
            {
                state.mKeys.push_back({key.mTime, {key.mHierarchyGain, key.mVelocityGain}});
                const auto bounds = ESM4::resolvePhysicalBlendKeyBounds(
                    {state.mTiming.mStartKey, state.mTiming.mStopKey}, state.mKeys);
                state.mTiming.mStartKey = bounds.mStartKey;
                state.mTiming.mStopKey = bounds.mStopKey;
            }
            mImpl->mBlendControllers.push_back({source.mRecord, source.mTargetRecord, std::move(state), body.mNodeRecord});
        }
        std::unordered_set<std::uint32_t> records;
        for (std::size_t i = 0; i < definition.mBodies.size(); ++i)
        {
            const auto& input = definition.mBodies[i];
            require(records.insert(input.mRecord).second, "duplicate body identity");
            validatePose(bodyPoses[i]);
            if (input.mUsesRigidBodyTransform)
                bodyTLocalRotation(input);
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
            mImpl->mBodies.push_back({input.mRecord, input.mNodeRecord, centerFrame, input.mLinearDamping,
                input.mAngularDamping, limits, std::move(body)});
            auto& owned = mImpl->mBodies.back();
            owned.mSceneOffset.mUsesRigidBodyTransform = input.mUsesRigidBodyTransform;
            owned.mSceneOffset.mTranslation = input.mTranslation;
            owned.mSceneOffset.mRotation = input.mRotation;
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
        // Every owned body, constraint and filter is prepared before any
        // registration. Deferred callers may prepare resource state first.
        if (publication == Publication::Immediate)
            (void)publish(world);
    }

    ActorRagdollPhysics::~ActorRagdollPhysics() = default;

    bool ActorRagdollPhysics::isPublished() const noexcept
    {
        return mImpl->mPublished && mImpl->worldAlive();
    }

    bool ActorRagdollPhysics::publish(btDynamicsWorld& world)
    {
        if (mImpl->mPublished || &world != &mImpl->mWorld || !mImpl->worldAlive())
            return false;
        if (mImpl->mNativeWorld)
        {
            const auto* native = dynamic_cast<NativeDynamicsWorld*>(&world);
            if (!native || native->lifetimeIdentity().lock() != mImpl->mWorldLifetime.lock())
                return false;
        }
        for (const auto& body : mImpl->mBodies)
            if (body.mBody->getBroadphaseHandle())
                return false;
        try
        {
            for (auto& body : mImpl->mBodies)
            {
                // A virtual World may publish its proxy and then throw.
                // Include the currently attempted body in rollback.
                ++mImpl->mRegisteredBodies;
                world.addRigidBody(body.mBody.get(), mImpl->mCollisionGroup, mImpl->mCollisionMask);
            }
            for (auto& constraint : mImpl->mConstraints)
            {
                ++mImpl->mRegisteredConstraints;
                world.addConstraint(constraint.get(), mImpl->mDisableLinkedCollisions);
            }
            if (auto* native = dynamic_cast<NativeDynamicsWorld*>(&world))
                native->registerNativeMotionOwner(this, mImpl->mCollisionObjects,
                    [this](float frame) { stepNativeKeyframedMotion(frame); });
            mImpl->mPublished = true;
        }
        catch (...)
        {
            // Registration uses strong vector insertion; an unsuccessful
            // native motion registration has not installed this owner.
            mImpl->removePublished();
            throw;
        }
        return true;
    }

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

    struct ActorRagdollPhysics::PreparedRestore::Data
    {
        const ActorRagdollPhysics* mOwner = nullptr;
        const Impl* mOwnerImpl = nullptr;
        std::vector<RagdollBodyState> mStates;
        std::optional<std::vector<RagdollNativePackedVelocityState>> mPacked;
        std::optional<std::vector<RagdollNativeMotionRequest>> mMotions;
        std::optional<std::vector<Impl::BlendTarget>> mTargets;
        std::optional<std::vector<RagdollNativeBlendControllerState>> mAuthored;
        std::optional<std::vector<RagdollNativeVelocityControllerState>> mGenerated;
    };
    ActorRagdollPhysics::PreparedRestore::PreparedRestore(std::unique_ptr<Data> data)
        : mData(std::move(data)) {}
    ActorRagdollPhysics::PreparedRestore::~PreparedRestore() = default;

    std::unique_ptr<ActorRagdollPhysics::PreparedRestore> ActorRagdollPhysics::prepareRestore(
        std::span<const RagdollBodyState> states) const
    {
        validateRestore(states);
        auto data = std::make_unique<PreparedRestore::Data>();
        data->mOwner = this; data->mOwnerImpl = mImpl.get();
        data->mStates.assign(states.begin(), states.end());
        return std::unique_ptr<PreparedRestore>(new PreparedRestore(std::move(data)));
    }

    std::unique_ptr<ActorRagdollPhysics::PreparedRestore> ActorRagdollPhysics::prepareRestore(
        std::span<const RagdollBodyState> states,
        std::span<const RagdollNativePackedVelocityState> packedVelocities) const
    {
        auto data = std::make_unique<PreparedRestore::Data>();
        data->mOwner = this; data->mOwnerImpl = mImpl.get();
        // Packed XYZ replaces the spatial velocity projection before validation.
        data->mStates = preparePackedRestore(states, packedVelocities);
        data->mPacked.emplace(packedVelocities.begin(), packedVelocities.end());
        return std::unique_ptr<PreparedRestore>(new PreparedRestore(std::move(data)));
    }

    std::unique_ptr<ActorRagdollPhysics::PreparedRestore> ActorRagdollPhysics::prepareRestore(
        std::span<const RagdollBodyState> states,
        std::span<const RagdollNativePackedVelocityState> packedVelocities,
        std::span<const RagdollNativeMotionRequest> motions) const
    {
        require(motions.size() == mImpl->mBodies.size(), "prepared pose motion count");
        for (std::size_t i = 0; i < motions.size(); ++i)
        {
            require(motions[i].mRecord == mImpl->mBodies[i].mRecord, "prepared pose motion identity");
            require(motions[i].mMotion == RagdollNativeMotion::Dynamic
                || motions[i].mMotion == RagdollNativeMotion::Keyframed, "invalid prepared pose motion");
        }
        auto prepared = prepareRestore(states, packedVelocities);
        prepared->mData->mMotions.emplace(motions.begin(), motions.end());
        return prepared;
    }

    std::unique_ptr<ActorRagdollPhysics::PreparedRestore> ActorRagdollPhysics::prepareRestore(
        std::span<const RagdollBodyState> states,
        std::span<const RagdollNativePackedVelocityState> packedVelocities,
        std::span<const RagdollNativeMotionRequest> motions,
        std::span<const RagdollNativeBlendState> blends) const
    {
        validateNativeBlendStates(blends);
        auto targets = mImpl->mBlendTargets;
        for (const auto& state : blends)
        {
            const auto target = std::find_if(targets.begin(), targets.end(),
                [&](const auto& value) { return value.mState.mBodyRecord == state.mBodyRecord; });
            target->mState = state;
        }
        auto prepared = prepareRestore(states, packedVelocities, motions);
        prepared->mData->mTargets = std::move(targets);
        return prepared;
    }

    std::unique_ptr<ActorRagdollPhysics::PreparedRestore> ActorRagdollPhysics::prepareRestore(
        std::span<const RagdollBodyState> states,
        std::span<const RagdollNativePackedVelocityState> packedVelocities,
        std::span<const RagdollNativeMotionRequest> motions,
        std::span<const RagdollNativeBlendState> blends,
        std::span<const RagdollNativeBlendControllerState> blendControllers,
        std::span<const RagdollNativeVelocityControllerState> velocityControllers) const
    {
        auto authored = prepareNativeBlendControllerRestore(blendControllers);
        auto generated = prepareNativeVelocityControllerRestore(velocityControllers);
        auto prepared = prepareRestore(states, packedVelocities, motions, blends);
        prepared->mData->mAuthored = std::move(authored);
        prepared->mData->mGenerated = std::move(generated);
        return prepared;
    }

    void ActorRagdollPhysics::commitRestore(PreparedRestore& prepared)
    {
        require(prepared.mData && prepared.mData->mOwner == this
            && prepared.mData->mOwnerImpl == mImpl.get(), "foreign or consumed prepared pose restore");
        auto& data = *prepared.mData;
        if (data.mMotions)
            for (std::size_t i = 0; i < data.mMotions->size(); ++i)
                if (mImpl->mBodies[i].mMotion != (*data.mMotions)[i].mMotion)
                    mImpl->setMotion(mImpl->mBodies[i], (*data.mMotions)[i].mMotion);
        if (data.mPacked) publishPackedRestore(data.mStates, *data.mPacked);
        else publishRestore(data.mStates);
        if (data.mTargets) mImpl->mBlendTargets.swap(*data.mTargets);
        if (data.mAuthored) mImpl->mBlendControllers.swap(*data.mAuthored);
        if (data.mGenerated) mImpl->mVelocityControllers.swap(*data.mGenerated);
        prepared.mData.reset();
    }

    void ActorRagdollPhysics::restore(std::span<const RagdollBodyState> states)
    {
        auto prepared = prepareRestore(states);
        commitRestore(*prepared);
    }

    void ActorRagdollPhysics::validateRestore(std::span<const RagdollBodyState> states) const
    {
        require(states.size() == mImpl->mBodies.size(), "state count");
        for (std::size_t i = 0; i < states.size(); ++i)
        {
            require(states[i].mRecord == mImpl->mBodies[i].mRecord, "state identity");
            validatePose(states[i].mPose);
            require(finite(states[i].mLinearVelocity) && finite(states[i].mAngularVelocity), "nonfinite velocity");
        }
    }

    void ActorRagdollPhysics::publishRestore(std::span<const RagdollBodyState> states)
    {
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
            if (mImpl->mPublished && mImpl->worldAlive()) mImpl->mWorld.updateSingleAabb(&body);
        }
    }

    std::vector<RagdollNativePackedVelocityState> ActorRagdollPhysics::captureNativePackedVelocities() const
    {
        std::vector<RagdollNativePackedVelocityState> result;
        result.reserve(mImpl->mBodies.size());
        for (const auto& owned : mImpl->mBodies)
        {
            RagdollNativePackedVelocityState state{owned.mRecord, {}};
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                state.mVelocities.mLinear[axis] = float(owned.mBody->getLinearVelocity()[axis] / mImpl->mLengthScale);
                state.mVelocities.mAngular[axis] = float(owned.mBody->getAngularVelocity()[axis]);
            }
            state.mVelocities.mLinear[3] = owned.mNativeLinearW;
            state.mVelocities.mAngular[3] = owned.mNativeAngularW;
            result.push_back(state);
        }
        return result;
    }

    namespace
    {
        std::pair<btVector3, btVector3> packedWorldVelocity(
            const ESM4::PhysicalWorldSceneVelocities& velocity, float lengthScale)
        {
            for (unsigned axis = 0; axis < 4; ++axis)
                require(std::isfinite(velocity.mLinear[axis]) && std::isfinite(velocity.mAngular[axis]),
                    "nonfinite native packed velocity");
            btVector3 linear(0, 0, 0), angular(0, 0, 0);
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                linear[axis] = btScalar(velocity.mLinear[axis]) * lengthScale;
                angular[axis] = velocity.mAngular[axis];
            }
            require(finite(linear) && finite(angular), "native packed velocity exceeds world domain");
            return {linear, angular};
        }
    }

    void ActorRagdollPhysics::restoreNativePackedVelocities(
        std::span<const RagdollNativePackedVelocityState> states)
    {
        require(states.size() == mImpl->mBodies.size(), "native packed velocity count");
        std::vector<std::pair<btVector3, btVector3>> pending;
        pending.reserve(states.size());
        for (std::size_t i = 0; i < states.size(); ++i)
        {
            require(states[i].mRecord == mImpl->mBodies[i].mRecord, "native packed velocity identity");
            pending.push_back(packedWorldVelocity(states[i].mVelocities, mImpl->mLengthScale));
        }
        for (std::size_t i = 0; i < states.size(); ++i)
        {
            auto& owned = mImpl->mBodies[i];
            owned.mBody->setLinearVelocity(pending[i].first);
            owned.mBody->setAngularVelocity(pending[i].second);
            owned.mNativeLinearW = states[i].mVelocities.mLinear[3];
            owned.mNativeAngularW = states[i].mVelocities.mAngular[3];
        }
    }

    std::vector<RagdollBodyState> ActorRagdollPhysics::preparePackedRestore(
        std::span<const RagdollBodyState> states,
        std::span<const RagdollNativePackedVelocityState> packedVelocities) const
    {
        require(states.size() == mImpl->mBodies.size() && packedVelocities.size() == states.size(),
            "packed pose state count");
        std::vector<RagdollBodyState> pending(states.begin(), states.end());
        for (std::size_t i = 0; i < pending.size(); ++i)
        {
            require(packedVelocities[i].mRecord == pending[i].mRecord, "packed pose state identity");
            const auto world = packedWorldVelocity(packedVelocities[i].mVelocities, mImpl->mLengthScale);
            pending[i].mLinearVelocity = world.first;
            pending[i].mAngularVelocity = world.second;
        }
        validateRestore(pending);
        return pending;
    }

    void ActorRagdollPhysics::publishPackedRestore(std::span<const RagdollBodyState> states,
        std::span<const RagdollNativePackedVelocityState> packedVelocities)
    {
        publishRestore(states);
        for (std::size_t i = 0; i < states.size(); ++i)
        {
            mImpl->mBodies[i].mNativeLinearW = packedVelocities[i].mVelocities.mLinear[3];
            mImpl->mBodies[i].mNativeAngularW = packedVelocities[i].mVelocities.mAngular[3];
        }
    }

    void ActorRagdollPhysics::restore(std::span<const RagdollBodyState> states,
        std::span<const RagdollNativePackedVelocityState> packedVelocities)
    {
        auto prepared = prepareRestore(states, packedVelocities);
        commitRestore(*prepared);
    }

    void ActorRagdollPhysics::restore(std::span<const RagdollBodyState> states,
        std::span<const RagdollNativePackedVelocityState> packedVelocities,
        std::span<const RagdollNativeMotionRequest> motions)
    {
        auto prepared = prepareRestore(states, packedVelocities, motions);
        commitRestore(*prepared);
    }

    void ActorRagdollPhysics::restore(std::span<const RagdollBodyState> states,
        std::span<const RagdollNativePackedVelocityState> packedVelocities,
        std::span<const RagdollNativeMotionRequest> motions,
        std::span<const RagdollNativeBlendState> blends)
    {
        auto prepared = prepareRestore(states, packedVelocities, motions, blends);
        commitRestore(*prepared);
    }

    void ActorRagdollPhysics::restore(std::span<const RagdollBodyState> states,
        std::span<const RagdollNativePackedVelocityState> packedVelocities,
        std::span<const RagdollNativeMotionRequest> motions,
        std::span<const RagdollNativeBlendState> blends,
        std::span<const RagdollNativeBlendControllerState> blendControllers,
        std::span<const RagdollNativeVelocityControllerState> velocityControllers)
    {
        auto prepared = prepareRestore(states, packedVelocities, motions, blends, blendControllers, velocityControllers);
        commitRestore(*prepared);
    }

    std::vector<std::uint32_t> ActorRagdollPhysics::synchronizeNativeWorldScenes(
        std::span<const RagdollNativeWorldSceneRequest> requests,
        const std::function<void(std::span<const std::uint32_t>)>& beforePublish)
    {
        struct Pending
        {
            Impl::Body* mOwned;
            ESM4::PhysicalWorldSceneVelocities mNative;
            std::pair<btVector3, btVector3> mWorld;
        };
        std::vector<Pending> pending;
        std::vector<std::uint32_t> written;
        std::unordered_set<std::uint32_t> selected;
        pending.reserve(requests.size());
        written.reserve(requests.size());
        for (const auto& request : requests)
        {
            require(selected.insert(request.mRecord).second, "duplicate native World scene body");
            const auto owned = std::find_if(mImpl->mBodies.begin(), mImpl->mBodies.end(),
                [&](const auto& body) { return body.mRecord == request.mRecord; });
            require(owned != mImpl->mBodies.end(), "unknown native World scene body");
            const auto velocity = ESM4::preparePhysicalWorldSceneVelocities(request.mInput);
            if (!velocity)
                continue;
            pending.push_back({&*owned, *velocity, packedWorldVelocity(*velocity, mImpl->mLengthScale)});
            written.push_back(request.mRecord);
        }
        if (beforePublish)
            beforePublish(written);
        for (const auto& change : pending)
        {
            auto& owned = *change.mOwned;
            owned.mBody->setLinearVelocity(change.mWorld.first);
            owned.mBody->setAngularVelocity(change.mWorld.second);
            owned.mNativeLinearW = change.mNative.mLinear[3];
            owned.mNativeAngularW = change.mNative.mAngular[3];
            mImpl->activateGroup(owned.mActivationGroup);
        }
        return written;
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
            const auto native = ragdollNativeSceneBodyTargetPose(request.mWorldPose, found->mSceneOffset);
            auto pose = btTransform(btQuaternion(native.mRotation[0], native.mRotation[1],
                native.mRotation[2], native.mRotation[3]), vector(native.mPosition));
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
            if (mImpl->mPublished && mImpl->worldAlive()) mImpl->mWorld.updateSingleAabb(&body);
        }
    }

    std::vector<RagdollNativeBlendPublication> ActorRagdollPhysics::updateNativeBlends(
        std::span<const RagdollNativeBlendUpdate> updates, float preparedFrameSeconds,
        std::uint32_t rawUpdateSelector, float nativeGravityZ)
    {
        return updateNativeBlendsImpl(updates, preparedFrameSeconds, rawUpdateSelector, nativeGravityZ, {});
    }

    std::vector<RagdollNativeBlendPublication> ActorRagdollPhysics::updateNativeBlendsImpl(
        std::span<const RagdollNativeBlendUpdate> updates, float preparedFrameSeconds,
        std::uint32_t rawUpdateSelector, float nativeGravityZ,
        const std::function<void(std::span<const RagdollNativeBlendPublication>)>& publishScene)
    {
        require(std::isfinite(nativeGravityZ), "invalid native blend gravity");
        ESM4::resolvePhysicalBlendDriveParameters(preparedFrameSeconds, 0, 0);
        struct Pending
        {
            Impl::Body* mOwned;
            RagdollNativeMotion mMotion;
            bool mChanged;
            bool mActivate;
            bool mResetPackedW;
            std::optional<btTransform> mPose;
            std::optional<std::pair<btVector3, btVector3>> mVelocities;
        };
        auto collisionTargets = mImpl->mBlendTargets;
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
            const auto collision = std::find_if(collisionTargets.begin(), collisionTargets.end(),
                [&](const auto& value) { return value.mState.mBodyRecord == update.mRecord; });
            const bool hasCollision = collision != collisionTargets.end();
            if (hasCollision)
            {
                collision->mState.mGains = {update.mHierarchyGain, update.mVelocityGain};
                collision->mState.mCollisionFlags = update.mCollisionFlags;
            }
            if (!dispatch)
                continue;
            const auto selectedMotion = dispatch->mMotion == ESM4::PhysicalBlendMotion::Keyframed
                ? RagdollNativeMotion::Keyframed : RagdollNativeMotion::Dynamic;
            // Original88F484 compares stored collision request, independently
            // of the actual body mode. A matching request skips conversion even
            // when an external body-mode change has made them disagree.
            const auto previousRequest = hasCollision ? collision->mState.mRequestedMotion
                : static_cast<std::uint32_t>(found->mMotion);
            const bool requestChanged = previousRequest != static_cast<std::uint32_t>(selectedMotion);
            const bool changed = requestChanged && found->mMotion != selectedMotion;
            const auto motion = changed ? selectedMotion : found->mMotion;
            Pending change{&*found, motion, changed,
                hasCollision && requestChanged && selectedMotion == RagdollNativeMotion::Dynamic,
                requestChanged && previousRequest != 6, std::nullopt, std::nullopt};
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
            // Before conversion89EAE0 chooses its setter using actual motion.
            // After conversion, the SceneToPhysics route repeats that choice.
            // Dynamic uses8A3900, whose no-native-World/authority case is a no-op;
            // this owner currently admits that boundary, not its World drive.
            const bool sync = (requestChanged && previousRequest == 6
                    && found->mMotion == RagdollNativeMotion::Keyframed)
                || (dispatch->mRoute == ESM4::PhysicalBlendRoute::SceneToPhysics
                    && motion == RagdollNativeMotion::Keyframed);
            if (sync)
            {
                // Flag0x20 without0x40 selects the distinct native World-driven
                // scene setter; this adapter has not admitted that path.
                require(!(update.mCollisionFlags & 0x20) || (update.mCollisionFlags & 0x40),
                    "unadmitted native World-driven scene synchronization");
                physical = ragdollNativeSceneBodyTargetPose(update.mAnimatedWorld, found->mSceneOffset);
                shapePose = btTransform(btQuaternion(physical.mRotation[0], physical.mRotation[1],
                    physical.mRotation[2], physical.mRotation[3]), vector(physical.mPosition) * mImpl->mLengthScale);
                centerPose = shapePose * found->mCenterFrame;
                validatePose(centerPose);
                change.mPose = centerPose;
            }
            const auto scenePhysical = ragdollNativeSceneTargetFromBodyPose(physical, found->mSceneOffset);
            auto linear = body.getLinearVelocity();
            auto angular = body.getAngularVelocity();
            if (requestChanged && previousRequest != 6)
            {
                linear.setZero();
                angular.setZero();
                change.mVelocities = std::pair{linear, angular};
            }
            auto flags = update.mCollisionFlags;
            if (hasCollision && requestChanged)
            {
                // Admitted native Dynamic archive type2 differs from requested
                // Dynamic1, so89ED20 runs its setter/flag path even without an
                // actual Dynamic/KEY handoff. A KEY6 getter already equal to6
                // skips that setter and preserves the flags.
                if (selectedMotion == RagdollNativeMotion::Dynamic)
                    flags |= 0x8;
                else if (found->mMotion == RagdollNativeMotion::Dynamic)
                    flags &= ~0x8;
            }
            else if (!hasCollision && changed)
                flags = motion == RagdollNativeMotion::Keyframed ? flags & ~0x8 : flags | 0x8;
            if (hasCollision)
            {
                collision->mState.mRequestedMotion = static_cast<std::uint32_t>(selectedMotion);
                collision->mState.mCollisionFlags = flags;
            }
            RagdollNativeBlendPublication publication{update.mRecord, flags, std::nullopt};
            if (dispatch->mRoute == ESM4::PhysicalBlendRoute::PhysicsToScene)
                publication.mSceneTarget = ragdollBoneWorldFromNativePose(scenePhysical);
            else if (dispatch->mRoute == ESM4::PhysicalBlendRoute::PoseAndVelocity)
            {
                const auto targets = ragdollNativeBlendPoseTargets(scenePhysical, update.mAnimatedWorld,
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
                currentCenter = ragdollNativeSceneCenterOfMass(currentCenter, physical.mRotation, found->mSceneOffset);
                const auto target = ragdollNativeTargetVelocities(localCenter, currentCenter,
                    scenePhysical.mRotation, targets.mDriveTarget, parameters.mInverseFrameSeconds,
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
        // Atomic renderer validation/publication runs only after the complete
        // physical batch has been prepared. No native computations remain after
        // the callback succeeds; controller metadata is already staged too.
        if (publishScene)
            publishScene(result);
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
                if (mImpl->mPublished && mImpl->worldAlive()) mImpl->mWorld.updateSingleAabb(&body);
            }
            if (change.mVelocities)
            {
                body.setLinearVelocity(change.mVelocities->first);
                body.setAngularVelocity(change.mVelocities->second);
            }
            if (change.mResetPackedW)
            {
                owned.mNativeLinearW = 0.f;
                owned.mNativeAngularW = 0.f;
            }
            if (change.mActivate || change.mChanged || change.mPose || change.mVelocities)
                mImpl->activateGroup(owned.mActivationGroup);
        }
        mImpl->mBlendTargets.swap(collisionTargets);
        return result;
    }

    std::vector<RagdollNativeKnockdownBlendDisposition> ActorRagdollPhysics::prepareNativeKnockdownBlends(
        std::span<const RagdollNativeKnockdownBlendRequest> requests)
    {
        std::vector<RagdollNativeKnockdownControllerSetupRequest> converted;
        converted.reserve(requests.size());
        for (const auto& request : requests)
            converted.push_back({request.mNodeRecord, {}, request.mDuration});
        return prepareNativeKnockdownControllerSetupImpl(converted, false, {});
    }

    std::vector<RagdollNativeHitBlendDisposition> ActorRagdollPhysics::prepareNativeHitBlends(
        std::span<const RagdollNativeHitBlendSetupRequest> requests)
    {
        auto prepared = prepareNativeHitBlendsImpl(requests);
        auto result = std::move(prepared.second);
        mImpl->mBlendControllers.swap(prepared.first);
        return result;
    }

    std::pair<std::vector<RagdollNativeBlendControllerState>, std::vector<RagdollNativeHitBlendDisposition>>
        ActorRagdollPhysics::prepareNativeHitBlendsImpl(
            std::span<const RagdollNativeHitBlendSetupRequest> requests) const
    {
        auto controllers = mImpl->mBlendControllers;
        std::vector<RagdollNativeHitBlendDisposition> result;
        result.reserve(requests.size());
        std::unordered_set<std::uint32_t> nodes;
        for (const auto& request : requests)
        {
            require(nodes.insert(request.mNodeRecord).second, "duplicate native HIT blend setup node");
            require(std::count_if(mImpl->mBodies.begin(), mImpl->mBodies.end(),
                        [&](const auto& body) { return body.mNodeRecord == request.mNodeRecord; }) == 1,
                "unknown or ambiguous native HIT blend setup node");
            const auto blend = std::find_if(mImpl->mBlendTargets.begin(), mImpl->mBlendTargets.end(),
                [&](const auto& value) { return value.mNode == request.mNodeRecord; });
            if (blend == mImpl->mBlendTargets.end())
            {
                result.push_back(RagdollNativeHitBlendDisposition::MissingBlend);
                continue;
            }
            const auto controller = std::find_if(controllers.begin(), controllers.end(),
                [&](const auto& value) { return value.mAttachedNode == request.mNodeRecord; });
            if (controller == controllers.end())
            {
                result.push_back(RagdollNativeHitBlendDisposition::MissingController);
                continue;
            }
            if (controller->mState.mSetupState > 1)
            {
                result.push_back(RagdollNativeHitBlendDisposition::StrongerSetup);
                continue;
            }
            controller->mState = ESM4::preparePhysicalHitBlendController(
                controller->mState, blend->mState.mGains, request.mConfiguredGains);
            result.push_back(RagdollNativeHitBlendDisposition::Started);
        }
        return {std::move(controllers), std::move(result)};
    }

    std::vector<RagdollNativeHitBlendDisposition> ActorRagdollPhysics::prepareNativeHitControllerSetup(
        std::span<const RagdollNativeHitBlendSetupRequest> blends,
        std::span<const RagdollNativeHitVelocitySetupRequest> velocities)
    {
        auto preparedBlends = prepareNativeHitBlendsImpl(blends);
        auto preparedVelocities = prepareNativeHitVelocityControllersImpl(velocities);
        // Move the result before publication. Returning this ordinary local
        // uses NRVO or vector's nonthrowing move, never a post-swap allocation.
        auto result = std::move(preparedBlends.second);
        mImpl->mBlendControllers.swap(preparedBlends.first);
        mImpl->mVelocityControllers.swap(preparedVelocities);
        return result;
    }

    void ActorRagdollPhysics::prepareNativeVelocityControllers(
        std::span<const RagdollNativeVelocitySetupRequest> requests)
    {
        auto controllers = mImpl->mVelocityControllers;
        std::unordered_set<std::uint32_t> nodes;
        for (const auto& request : requests)
        {
            require(nodes.insert(request.mNodeRecord).second, "duplicate native velocity setup node");
            require(std::count_if(mImpl->mBodies.begin(), mImpl->mBodies.end(),
                        [&](const auto& body) { return body.mNodeRecord == request.mNodeRecord; }) == 1,
                "unknown or ambiguous native velocity setup node");
            const auto body = std::find_if(mImpl->mBodies.begin(), mImpl->mBodies.end(),
                [&](const auto& value) { return value.mNodeRecord == request.mNodeRecord; });
            const auto controller = std::find_if(controllers.begin(), controllers.end(),
                [&](const auto& value) { return value.mAttachedNode == request.mNodeRecord; });
            const auto previous = controller == controllers.end() ? std::nullopt
                : std::optional<ESM4::PhysicalVelocityControllerState>{controller->mState};
            const auto state = ESM4::preparePhysicalVelocityController(previous, request.mSourceVector,
                request.mDuration, true, ragdollNativeInverseMass(float(body->mDynamicMass)), body->currentNativeLinearDamping());
            if (controller == controllers.end())
                controllers.push_back({request.mNodeRecord, request.mNodeRecord, state, true});
            else
                controller->mState = state;
        }
        mImpl->mVelocityControllers.swap(controllers);
    }

    void ActorRagdollPhysics::prepareNativeHitVelocityControllers(
        std::span<const RagdollNativeHitVelocitySetupRequest> requests)
    {
        auto prepared = prepareNativeHitVelocityControllersImpl(requests);
        mImpl->mVelocityControllers.swap(prepared);
    }

    std::vector<RagdollNativeVelocityControllerState> ActorRagdollPhysics::prepareNativeHitVelocityControllersImpl(
        std::span<const RagdollNativeHitVelocitySetupRequest> requests) const
    {
        auto controllers = mImpl->mVelocityControllers;
        std::unordered_set<std::uint32_t> nodes;
        for (const auto& request : requests)
        {
            require(nodes.insert(request.mNodeRecord).second, "duplicate native HIT velocity setup node");
            require(std::count_if(mImpl->mBodies.begin(), mImpl->mBodies.end(),
                        [&](const auto& body) { return body.mNodeRecord == request.mNodeRecord; }) == 1,
                "unknown or ambiguous native HIT velocity setup node");
            const auto body = std::find_if(mImpl->mBodies.begin(), mImpl->mBodies.end(),
                [&](const auto& value) { return value.mNodeRecord == request.mNodeRecord; });
            const auto controller = std::find_if(controllers.begin(), controllers.end(),
                [&](const auto& value) { return value.mAttachedNode == request.mNodeRecord; });
            const auto previous = controller == controllers.end() ? std::nullopt
                : std::optional<ESM4::PhysicalVelocityControllerState>{controller->mState};
            const auto state = ESM4::preparePhysicalHitVelocityController(previous, request.mSourceVector,
                true, ragdollNativeInverseMass(float(body->mDynamicMass)),
                body->currentNativeLinearDamping(), request.mResolvedMassMultiplier);
            if (controller == controllers.end())
                controllers.push_back({request.mNodeRecord, request.mNodeRecord, state, true});
            else
                controller->mState = state;
        }
        return controllers;
    }

    std::vector<RagdollNativeVelocityControllerState> ActorRagdollPhysics::captureNativeVelocityControllers() const
    {
        return mImpl->mVelocityControllers;
    }

    std::vector<RagdollNativeVelocityControllerState> ActorRagdollPhysics::prepareNativeVelocityControllerRestore(
        std::span<const RagdollNativeVelocityControllerState> controllers) const
    {
        std::vector<RagdollNativeVelocityControllerState> next(controllers.begin(), controllers.end());
        std::unordered_set<std::uint32_t> nodes;
        const auto validNode = [&](std::uint32_t node) {
            return std::count_if(mImpl->mBodies.begin(), mImpl->mBodies.end(),
                [&](const auto& body) { return body.mNodeRecord == node; }) == 1;
        };
        for (const auto& controller : next)
        {
            require(nodes.insert(controller.mAttachedNode).second, "duplicate native velocity restore node");
            require(validNode(controller.mAttachedNode), "unknown or ambiguous native velocity attachment");
            require(!controller.mTargetNode || validNode(*controller.mTargetNode),
                "unknown or ambiguous native velocity target");
            const auto& state = controller.mState;
            require(std::isfinite(state.mTiming.mFrequency) && std::isfinite(state.mTiming.mPhase)
                    && std::isfinite(state.mTiming.mStartKey) && std::isfinite(state.mTiming.mStopKey)
                    && state.mTiming.mStartKey <= state.mTiming.mStopKey,
                "invalid native velocity restore timing");
            require(std::isfinite(state.mClock.mStartTime) && std::isfinite(state.mClock.mPreviousTime)
                    && std::isfinite(state.mClock.mElapsed) && std::isfinite(state.mFrameDelta)
                    && state.mFrameDelta >= 0.f,
                "invalid native velocity restore clock/delta");
            require(std::all_of(state.mForceVector.begin(), state.mForceVector.end(),
                        [](float value) { return std::isfinite(value); }),
                "invalid native velocity restore vector");
        }
        return next;
    }

    void ActorRagdollPhysics::restoreNativeVelocityControllers(
        std::span<const RagdollNativeVelocityControllerState> controllers)
    {
        auto next = prepareNativeVelocityControllerRestore(controllers);
        mImpl->mVelocityControllers.swap(next);
    }

    std::vector<RagdollNativeBlendControllerState> ActorRagdollPhysics::prepareNativeBlendControllerRestore(
        std::span<const RagdollNativeBlendControllerState> controllers) const
    {
        require(controllers.size() == mImpl->mBlendControllers.size(), "incomplete native authored controller snapshot");
        auto next = mImpl->mBlendControllers;
        std::unordered_set<std::uint32_t> selected;
        const auto validNode = [&](std::uint32_t node) {
            return std::count_if(mImpl->mBodies.begin(), mImpl->mBodies.end(),
                [&](const auto& body) { return body.mNodeRecord == node; }) == 1;
        };
        for (const auto& controller : controllers)
        {
            require(selected.insert(controller.mRecord).second, "duplicate native authored controller record");
            const auto target = std::find_if(next.begin(), next.end(),
                [&](const auto& value) { return value.mRecord == controller.mRecord; });
            require(target != next.end(), "unknown native authored controller record");
            require(controller.mAttachedNode == target->mAttachedNode, "native authored controller attachment mismatch");
            require(!controller.mTargetNode || validNode(*controller.mTargetNode),
                "unknown or ambiguous native authored controller target");
            const auto& state = controller.mState;
            require(std::isfinite(state.mTiming.mFrequency) && std::isfinite(state.mTiming.mPhase)
                    && std::isfinite(state.mTiming.mStartKey) && std::isfinite(state.mTiming.mStopKey),
                "invalid native authored controller timing");
            require(std::isfinite(state.mClock.mStartTime) && std::isfinite(state.mClock.mPreviousTime)
                    && std::isfinite(state.mClock.mElapsed)
                    && std::isfinite(state.mCachedGains.mHierarchy) && std::isfinite(state.mCachedGains.mVelocity),
                "invalid native authored controller clock or cached gains");
            require(state.mKeys.size() <= std::numeric_limits<std::uint32_t>::max()
                    && (state.mKeys.size() < 2 || state.mCursor < state.mKeys.size() - 1),
                "invalid native authored controller key count or cursor");
            for (std::size_t i = 0; i < state.mKeys.size(); ++i)
            {
                const auto& key = state.mKeys[i];
                require(std::isfinite(key.mTime) && std::isfinite(key.mGains.mHierarchy) && std::isfinite(key.mGains.mVelocity)
                        && (i == 0 || state.mKeys[i - 1].mTime <= key.mTime),
                    "invalid native authored controller key or order");
            }
            // Keep raw bounds/flags/cursor/setup; loading and inactive native
            // states need not satisfy the admission for an active clock tick.
            *target = controller;
        }
        return next;
    }

    std::vector<RagdollNativeControllerReference> ActorRagdollPhysics::captureNativeControllerOrder(
        std::span<const std::uint32_t> nodeOrder) const
    {
        std::vector<RagdollNativeControllerReference> result;
        result.reserve(mImpl->mBlendControllers.size() + mImpl->mVelocityControllers.size());
        std::unordered_set<std::uint32_t> nodes;
        for (const auto node : nodeOrder)
        {
            require(nodes.insert(node).second, "duplicate native controller traversal node");
            require(std::count_if(mImpl->mBodies.begin(), mImpl->mBodies.end(),
                        [&](const auto& body) { return body.mNodeRecord == node; }) == 1,
                "unknown or ambiguous native controller traversal node");
            const auto velocity = std::find_if(mImpl->mVelocityControllers.begin(), mImpl->mVelocityControllers.end(),
                [&](const auto& value) { return value.mAttachedNode == node; });
            const auto blend = std::find_if(mImpl->mBlendControllers.begin(), mImpl->mBlendControllers.end(),
                [&](const auto& value) { return value.mAttachedNode == node; });
            if (velocity != mImpl->mVelocityControllers.end() && velocity->mPrecedesBlend)
                result.push_back({RagdollNativeControllerKind::Velocity, node});
            if (blend != mImpl->mBlendControllers.end())
                result.push_back({RagdollNativeControllerKind::Blend, blend->mRecord});
            if (velocity != mImpl->mVelocityControllers.end() && !velocity->mPrecedesBlend)
                result.push_back({RagdollNativeControllerKind::Velocity, node});
        }
        return result;
    }

    void ActorRagdollPhysics::advanceNativePhysicalControllers(
        std::span<const RagdollNativeControllerReference> controllerOrder, float inputTime,
        ESM4::PhysicalBlendTimeCache& sharedTimeCache)
    {
        require(controllerOrder.size() == mImpl->mBlendControllers.size() + mImpl->mVelocityControllers.size(),
            "native physical controller order must be complete");
        std::unordered_set<std::uint64_t> selected;
        for (const auto& reference : controllerOrder)
        {
            require(reference.mKind == RagdollNativeControllerKind::Blend
                    || reference.mKind == RagdollNativeControllerKind::Velocity,
                "unknown native physical controller kind");
            const auto identity = (std::uint64_t(reference.mKind) << 32) | reference.mIdentity;
            require(selected.insert(identity).second, "duplicate native physical controller reference");
            if (reference.mKind == RagdollNativeControllerKind::Blend)
                require(std::any_of(mImpl->mBlendControllers.begin(), mImpl->mBlendControllers.end(),
                            [&](const auto& value) { return value.mRecord == reference.mIdentity; }),
                    "unknown native physical blend controller");
            else
                require(std::any_of(mImpl->mVelocityControllers.begin(), mImpl->mVelocityControllers.end(),
                            [&](const auto& value) { return value.mAttachedNode == reference.mIdentity; }),
                    "unknown native physical velocity controller");
        }
        auto blends = mImpl->mBlendControllers;
        auto velocities = mImpl->mVelocityControllers;
        auto targets = mImpl->mBlendTargets;
        auto cache = sharedTimeCache;
        std::vector<RagdollNativeForceRequest> forces;
        forces.reserve(velocities.size());
        for (const auto& reference : controllerOrder)
        {
            if (reference.mKind == RagdollNativeControllerKind::Blend)
            {
                const auto controller = std::find_if(blends.begin(), blends.end(),
                    [&](const auto& value) { return value.mRecord == reference.mIdentity; });
                const auto target = std::find_if(targets.begin(), targets.end(),
                    [&](const auto& value) { return controller->mTargetNode && value.mNode == *controller->mTargetNode; });
                const auto gains = target == targets.end() ? std::nullopt
                    : std::optional<ESM4::PhysicalBlendGains>{target->mState.mGains};
                const auto velocity = std::find_if(velocities.begin(), velocities.end(),
                    [&](const auto& value) {
                        return controller->mTargetNode && value.mAttachedNode == *controller->mTargetNode;
                    });
                auto update = ESM4::advancePhysicalBlendController(controller->mState, cache,
                    controller->mTargetNode.has_value(), gains, velocity != velocities.end(), inputTime);
                controller->mState = std::move(update.mController);
                cache = update.mTimeCache;
                if (target != targets.end())
                    target->mState.mGains = *update.mTargetGains;
                if (update.mRemoveVelocityController)
                    velocities.erase(velocity);
            }
            else
            {
                const auto controller = std::find_if(velocities.begin(), velocities.end(),
                    [&](const auto& value) { return value.mAttachedNode == reference.mIdentity; });
                // Original47C930 reads next after Update. A controller detached
                // by an earlier blend finish is not subsequently updated.
                if (controller == velocities.end())
                    continue;
                const auto target = std::find_if(targets.begin(), targets.end(),
                    [&](const auto& value) { return controller->mTargetNode && value.mNode == *controller->mTargetNode; });
                const auto body = std::find_if(mImpl->mBodies.begin(), mImpl->mBodies.end(),
                    [&](const auto& value) { return controller->mTargetNode && value.mNodeRecord == *controller->mTargetNode; });
                const auto gain = target == targets.end() ? std::nullopt
                    : std::optional<float>{target->mState.mGains.mHierarchy};
                auto update = ESM4::advancePhysicalVelocityController(controller->mState, cache,
                    controller->mTargetNode.has_value(), gain, body != mImpl->mBodies.end(), inputTime);
                controller->mState = update.mController;
                cache = update.mTimeCache;
                if (update.mForce)
                    forces.push_back({body->mRecord,
                        {(*update.mForce)[0], (*update.mForce)[1], (*update.mForce)[2]}, update.mController.mFrameDelta,
                        (*update.mForce)[3]});
            }
        }
        // Force preparation validates every used body/result before publishing
        // velocities or waking groups. Logical candidates are already allocated.
        applyNativeForcesImpl(forces, true);
        mImpl->mBlendControllers.swap(blends);
        mImpl->mVelocityControllers.swap(velocities);
        mImpl->mBlendTargets.swap(targets);
        sharedTimeCache = cache;
    }

    std::vector<RagdollNativeKnockdownBlendDisposition> ActorRagdollPhysics::prepareNativeKnockdownControllerSetup(
        std::span<const RagdollNativeKnockdownControllerSetupRequest> requests,
        RagdollNativePassOutSettings settings)
    {
        return prepareNativeKnockdownControllerSetupImpl(requests, true, settings);
    }

    std::vector<RagdollNativeKnockdownBlendDisposition> ActorRagdollPhysics::prepareNativeKnockdownControllerSetupImpl(
        std::span<const RagdollNativeKnockdownControllerSetupRequest> requests, bool includeVelocity,
        RagdollNativePassOutSettings settings)
    {
        auto controllers = mImpl->mBlendControllers;
        auto velocities = mImpl->mVelocityControllers;
        struct PendingMotion
        {
            Impl::Body* mOwned;
            RagdollNativeMotion mMotion;
        };
        std::vector<PendingMotion> motionChanges;
        motionChanges.reserve(requests.size());
        std::vector<RagdollNativeKnockdownBlendDisposition> result;
        result.reserve(requests.size());
        std::unordered_set<std::uint32_t> nodes;
        for (const auto& request : requests)
        {
            require(nodes.insert(request.mNodeRecord).second, "duplicate native knockdown setup node");
            require(std::count_if(mImpl->mBodies.begin(), mImpl->mBodies.end(),
                        [&](const auto& body) { return body.mNodeRecord == request.mNodeRecord; }) == 1,
                "unknown or ambiguous native knockdown setup node");
            const auto blend = std::find_if(mImpl->mBlendTargets.begin(), mImpl->mBlendTargets.end(),
                [&](const auto& value) { return value.mNode == request.mNodeRecord; });
            if (blend == mImpl->mBlendTargets.end())
            {
                result.push_back(RagdollNativeKnockdownBlendDisposition::MissingBlend);
                continue;
            }
            const auto owned = std::find_if(mImpl->mBodies.begin(), mImpl->mBodies.end(),
                [&](const auto& value) { return value.mNodeRecord == request.mNodeRecord; });
            auto preparedMotion = owned->mMotion;
            // Original88F040 synchronizes collision+1C before controller lookup
            // and disabled duration handling, without changing flags/request.
            if (blend->mState.mRequestedMotion == 6)
                preparedMotion = RagdollNativeMotion::Keyframed;
            else if (blend->mState.mRequestedMotion == 1)
                preparedMotion = RagdollNativeMotion::Dynamic;
            if (preparedMotion != owned->mMotion)
                motionChanges.push_back({&*owned, preparedMotion});
            const auto controller = std::find_if(controllers.begin(), controllers.end(),
                [&](const auto& value) { return value.mAttachedNode == request.mNodeRecord; });
            if (controller == controllers.end())
            {
                result.push_back(RagdollNativeKnockdownBlendDisposition::MissingController);
                continue;
            }
            require(std::isfinite(request.mDuration), "nonfinite native knockdown setup duration");
            if (request.mDuration < 0.f)
            {
                result.push_back(RagdollNativeKnockdownBlendDisposition::Disabled);
                continue;
            }
            auto& state = controller->mState;
            const auto setup = ESM4::preparePhysicalKnockdownBlend(
                blend->mState.mGains, request.mDuration, 0.f, state.mTiming.mFlags, state.mClock);
            state.mKeys.assign(setup.mKeys.begin(), setup.mKeys.end());
            // Native insertion updates bounds, then8AB440 overwrites them.
            state.mTiming = {setup.mControllerFlags, 1.f, 0.f, setup.mStartKey, setup.mStopKey};
            state.mClock = setup.mClock;
            state.mCursor = 0;
            state.mCachedGains = {-1.f, -1.f};
            state.mSetupState = 2;
            if (includeVelocity && std::none_of(velocities.begin(), velocities.end(),
                    [&](const auto& value) { return value.mAttachedNode == request.mNodeRecord; }))
            {
                require(std::isfinite(settings.mForce), "nonfinite native pass-out force");
                require(std::isfinite(settings.mTime) && settings.mTime >= 0.f, "invalid native pass-out duration");
                std::array<float, 4> source;
                for (unsigned axis = 0; axis < 3; ++axis)
                {
                    require(std::isfinite(request.mWorldVector[axis]), "nonfinite native Down world vector");
                    // Original4707B0 binary32 products, then4529E0 stores with
                    // the separately stored native world-to-length constant.
                    const float scaled = float(double(request.mWorldVector[axis]) * double(settings.mForce));
                    require(std::isfinite(scaled), "native Down world vector product overflow");
                    source[axis] = float(double(scaled) * double(0.1428767293691635f));
                    require(std::isfinite(source[axis]), "native Down vector conversion overflow");
                }
                // Original stack scratch retains the second blend key time in
                // this fourth lane. Preserve it in the owned controller vector.
                source[3] = request.mDuration;
                const auto velocity = ESM4::preparePhysicalVelocityController(std::nullopt, source,
                    settings.mTime, true, ragdollNativeInverseMass(float(owned->mDynamicMass)),
                    preparedMotion == RagdollNativeMotion::Keyframed ? 0.f : owned->mLinearDamping);
                velocities.push_back({request.mNodeRecord, request.mNodeRecord, velocity, true});
            }
            result.push_back(RagdollNativeKnockdownBlendDisposition::Started);
        }
        // All input validation and controller allocation precede physical mode
        // publication. Native conversion preserves pose and stored velocities.
        for (const auto& change : motionChanges)
            mImpl->setMotion(*change.mOwned, change.mMotion);
        for (const auto& change : motionChanges)
            if (change.mMotion == RagdollNativeMotion::Dynamic)
                mImpl->activateGroup(change.mOwned->mActivationGroup);
        mImpl->mBlendControllers.swap(controllers);
        if (includeVelocity)
            mImpl->mVelocityControllers.swap(velocities);
        return result;
    }

    std::vector<RagdollNativeBlendControllerState> ActorRagdollPhysics::captureNativeBlendControllers() const
    {
        return mImpl->mBlendControllers;
    }

    std::vector<RagdollNativeBlendState> ActorRagdollPhysics::captureNativeBlendStates() const
    {
        std::vector<RagdollNativeBlendState> result;
        result.reserve(mImpl->mBlendTargets.size());
        for (const auto& target : mImpl->mBlendTargets)
            result.push_back(target.mState);
        return result;
    }

    void ActorRagdollPhysics::validateNativeBlendStates(std::span<const RagdollNativeBlendState> states) const
    {
        require(states.size() == mImpl->mBlendTargets.size(), "incomplete native blend collision snapshot");
        std::unordered_set<std::uint32_t> selected;
        for (const auto& state : states)
        {
            require(selected.insert(state.mBodyRecord).second, "duplicate native blend collision body");
            const auto target = std::find_if(mImpl->mBlendTargets.begin(), mImpl->mBlendTargets.end(),
                [&](const auto& value) { return value.mState.mBodyRecord == state.mBodyRecord; });
            require(target != mImpl->mBlendTargets.end(), "unknown native blend collision body");
            require(std::isfinite(state.mGains.mHierarchy) && std::isfinite(state.mGains.mVelocity),
                "nonfinite native blend collision gains");
        }
    }

    void ActorRagdollPhysics::restoreNativeBlendStates(std::span<const RagdollNativeBlendState> states)
    {
        validateNativeBlendStates(states);
        auto targets = mImpl->mBlendTargets;
        for (const auto& state : states)
        {
            const auto target = std::find_if(targets.begin(), targets.end(),
                [&](const auto& value) { return value.mState.mBodyRecord == state.mBodyRecord; });
            // Raw request/flags/gains stay independent of actual body modes.
            target->mState = state;
        }
        mImpl->mBlendTargets.swap(targets);
    }

    std::vector<RagdollNativeBlendPublication> ActorRagdollPhysics::updateNativeBlendControllers(
        std::span<const RagdollNativeBlendControllerTarget> targets, float inputTime,
        ESM4::PhysicalBlendTimeCache& sharedTimeCache, float preparedFrameSeconds,
        std::uint32_t rawUpdateSelector, float nativeGravityZ)
    {
        return updateNativeBlendControllersImpl(targets, {}, inputTime, sharedTimeCache,
            preparedFrameSeconds, rawUpdateSelector, nativeGravityZ, {});
    }

    std::vector<RagdollNativeBlendPublication> ActorRagdollPhysics::updateNativeBlendControllersImpl(
        std::span<const RagdollNativeBlendControllerTarget> targets,
        std::span<const RagdollBoneWorldPose> completeBones, float inputTime,
        ESM4::PhysicalBlendTimeCache& sharedTimeCache, float preparedFrameSeconds,
        std::uint32_t rawUpdateSelector, float nativeGravityZ,
        const std::function<void(std::span<const RagdollNativeBlendPublication>)>& publishScene)
    {
        require(std::isfinite(nativeGravityZ), "invalid owned native controller gravity");
        ESM4::resolvePhysicalBlendDriveParameters(preparedFrameSeconds, 0, 0);
        auto nextControllers = mImpl->mBlendControllers;
        auto nextTargets = mImpl->mBlendTargets;
        auto nextCache = sharedTimeCache;
        std::vector<RagdollNativeBlendUpdate> bodyUpdates;
        bodyUpdates.reserve(targets.size());
        std::unordered_set<std::uint32_t> selectedControllers, selectedBodies;
        for (const auto& request : targets)
        {
            require(selectedControllers.insert(request.mControllerRecord).second,
                "duplicate owned native controller request");
            const auto controller = std::find_if(nextControllers.begin(), nextControllers.end(),
                [&](const auto& value) { return value.mRecord == request.mControllerRecord; });
            require(controller != nextControllers.end(), "unknown owned native controller identity");
            const auto target = std::find_if(nextTargets.begin(), nextTargets.end(),
                [&](const auto& value) { return controller->mTargetNode && value.mNode == *controller->mTargetNode; });
            const auto gains = target == nextTargets.end() ? std::nullopt
                : std::optional<ESM4::PhysicalBlendGains>{target->mState.mGains};
            auto update = ESM4::advancePhysicalBlendController(controller->mState, nextCache,
                controller->mTargetNode.has_value(), gains, false, inputTime);
            controller->mState = std::move(update.mController);
            nextCache = update.mTimeCache;
            if (target == nextTargets.end())
                continue;
            require(selectedBodies.insert(target->mState.mBodyRecord).second,
                "unadmitted multiple controllers for one physical target");
            target->mState.mGains = *update.mTargetGains;
            bodyUpdates.push_back({target->mState.mBodyRecord, request.mAnimatedWorld,
                target->mState.mGains.mHierarchy, target->mState.mGains.mVelocity, target->mState.mCollisionFlags});
        }
        if (!completeBones.empty())
        {
            // Controller traversal and collision-object traversal are separate.
            // Every physical target uses its final owned gain, including targets
            // with no controller. The complete frame supplies collision order.
            bodyUpdates.clear();
            bodyUpdates.reserve(nextTargets.size());
            for (const auto& bone : completeBones)
            {
                const auto target = std::find_if(nextTargets.begin(), nextTargets.end(),
                    [&](const auto& value) { return value.mNode == bone.mNodeRecord; });
                if (target != nextTargets.end())
                    bodyUpdates.push_back({target->mState.mBodyRecord, bone.mPose,
                        target->mState.mGains.mHierarchy, target->mState.mGains.mVelocity,
                        target->mState.mCollisionFlags});
            }
        }
        // Original88F4C5 retains requested motion independently of the body's
        // actual motion. Stage that collision metadata before the scene hook;
        // skipped selector branches retain the previous request.
        for (const auto& update : bodyUpdates)
        {
            const auto dispatch = ESM4::resolvePhysicalBlendDispatch(update.mHierarchyGain,
                update.mVelocityGain, update.mCollisionFlags, rawUpdateSelector);
            if (!dispatch)
                continue;
            const auto target = std::find_if(nextTargets.begin(), nextTargets.end(),
                [&](const auto& value) { return value.mState.mBodyRecord == update.mRecord; });
            target->mState.mRequestedMotion = static_cast<std::uint32_t>(dispatch->mMotion);
        }
        // The existing body bridge stages every computation before any body
        // mutation. All controller/target/cache allocations are already done.
        auto publications = updateNativeBlendsImpl(bodyUpdates, preparedFrameSeconds,
            rawUpdateSelector, nativeGravityZ, publishScene);
        for (const auto& publication : publications)
            for (auto& target : nextTargets)
                if (target.mState.mBodyRecord == publication.mRecord)
                    target.mState.mCollisionFlags = publication.mCollisionFlags;
        mImpl->mBlendControllers.swap(nextControllers);
        mImpl->mBlendTargets.swap(nextTargets);
        sharedTimeCache = nextCache;
        return publications;
    }

    std::vector<RagdollNativeBlendPublication> ActorRagdollPhysics::updateNativeBlendFrame(
        std::span<const RagdollBoneWorldPose> bones, std::span<const std::uint32_t> controllerOrder,
        float inputTime, ESM4::PhysicalBlendTimeCache& sharedTimeCache, float preparedFrameSeconds,
        std::uint32_t rawUpdateSelector, float nativeGravityZ,
        const std::function<void(std::span<const RagdollNativeBlendPublication>)>& publishScene)
    {
        require(bones.size() == mImpl->mBodies.size(), "incomplete native blend frame bones");
        require(controllerOrder.size() == mImpl->mBlendControllers.size(),
            "incomplete native blend frame controllers");
        std::unordered_map<std::uint32_t, const osg::Matrixf*> poses;
        for (const auto& bone : bones)
            require(poses.emplace(bone.mNodeRecord, &bone.mPose).second, "duplicate native blend frame bone");
        std::unordered_set<std::uint32_t> ownedNodes;
        for (const auto& body : mImpl->mBodies)
        {
            require(ownedNodes.insert(body.mNodeRecord).second, "ambiguous native blend frame body node");
            require(poses.contains(body.mNodeRecord), "missing native blend frame body node");
        }
        std::vector<RagdollNativeBlendControllerTarget> requests;
        requests.reserve(controllerOrder.size());
        for (const auto record : controllerOrder)
        {
            const auto controller = std::find_if(mImpl->mBlendControllers.begin(), mImpl->mBlendControllers.end(),
                [&](const auto& value) { return value.mRecord == record; });
            require(controller != mImpl->mBlendControllers.end(), "unknown native blend frame controller");
            requests.push_back({record, controller->mTargetNode ? *poses.at(*controller->mTargetNode)
                : osg::Matrixf::identity()});
        }
        return updateNativeBlendControllersImpl(requests, bones, inputTime, sharedTimeCache,
            preparedFrameSeconds, rawUpdateSelector, nativeGravityZ, publishScene);
    }

    void ActorRagdollPhysics::applyNativeForces(std::span<const RagdollNativeForceRequest> requests)
    {
        applyNativeForcesImpl(requests, false);
    }

    void ActorRagdollPhysics::applyNativeForcesImpl(
        std::span<const RagdollNativeForceRequest> requests, bool allowRepeatedBodies)
    {
        struct Pending
        {
            Impl::Body* mOwned;
            btVector3 mLinear;
            float mLinearW;
        };
        std::vector<Pending> pending;
        pending.reserve(requests.size());
        std::unordered_set<std::uint32_t> records;
        for (const auto& request : requests)
        {
            const bool unique = records.insert(request.mRecord).second;
            require(unique || allowRepeatedBodies, "duplicate native force body");
            const auto owned = std::find_if(mImpl->mBodies.begin(), mImpl->mBodies.end(),
                [&](const auto& body) { return body.mRecord == request.mRecord; });
            require(owned != mImpl->mBodies.end(), "unknown native force body");
            const auto prior = std::find_if(pending.rbegin(), pending.rend(),
                [&](const auto& value) { return value.mOwned == &*owned; });
            auto linear = prior == pending.rend() ? owned->mBody->getLinearVelocity() : prior->mLinear;
            float linearW = prior == pending.rend() ? owned->mNativeLinearW : prior->mLinearW;
            if (owned->mMotion == RagdollNativeMotion::Dynamic)
            {
                std::array<float, 4> current{}, force{};
                for (unsigned axis = 0; axis < 3; ++axis)
                {
                    current[axis] = float(linear[axis] / mImpl->mLengthScale);
                    force[axis] = request.mForce[axis];
                }
                current[3] = linearW;
                force[3] = request.mForceW;
                const auto output = ragdollNativeLinearVelocityAfterForce(current,
                    ragdollNativeInverseMass(float(owned->mDynamicMass)), request.mFrameSeconds, force);
                for (unsigned axis = 0; axis < 3; ++axis)
                    linear[axis] = btScalar(output[axis]) * mImpl->mLengthScale;
                linearW = output[3];
                require(finite(linear), "native force exceeds world velocity domain");
            }
            pending.push_back({&*owned, linear, linearW});
        }
        for (const auto& next : pending)
        {
            mImpl->activateGroup(next.mOwned->mActivationGroup);
            if (next.mOwned->mMotion == RagdollNativeMotion::Dynamic)
            {
                next.mOwned->mBody->setLinearVelocity(next.mLinear);
                next.mOwned->mNativeLinearW = next.mLinearW;
            }
        }
    }

    void ActorRagdollPhysics::applyImpulse(std::size_t body, const btVector3& impulse, const btVector3& worldPoint)
    {
        require(body < mImpl->mBodies.size() && finite(impulse) && finite(worldPoint), "invalid impulse");
        auto& target = *mImpl->mBodies[body].mBody;
        mImpl->activateGroup(mImpl->mBodies[body].mActivationGroup);
        target.applyImpulse(impulse, worldPoint - target.getCenterOfMassPosition());
    }

    void ActorRagdollPhysics::applyNativePackedVelocityStep(float frameSeconds,
        std::span<const std::array<float, 4>> nativeLinearDeltas)
    {
        require(nativeLinearDeltas.size() == mImpl->mBodies.size(), "velocity delta count");
        require(std::isfinite(frameSeconds) && frameSeconds >= 0, "invalid frame duration");
        const auto states = captureNativePackedVelocities();
        struct Pending
        {
            Impl::Body* mOwned;
            ESM4::PhysicalWorldSceneVelocities mNative;
            std::pair<btVector3, btVector3> mWorld;
        };
        std::vector<Pending> pending;
        pending.reserve(states.size());
        for (std::size_t i = 0; i < states.size(); ++i)
        {
            auto& owned = mImpl->mBodies[i];
            // Sleeping islands and KEY bodies do not consume dynamic deltas.
            if (owned.mMotion != RagdollNativeMotion::Dynamic || !owned.mBody->isActive())
                continue;
            const auto output = ragdollNativePackedVelocityStep(states[i].mVelocities,
                owned.mLimits, frameSeconds, nativeLinearDeltas[i]);
            pending.push_back({&owned, output, packedWorldVelocity(output, mImpl->mLengthScale)});
        }
        for (const auto& next : pending)
        {
            next.mOwned->mBody->setLinearVelocity(next.mWorld.first);
            next.mOwned->mBody->setAngularVelocity(next.mWorld.second);
            next.mOwned->mNativeLinearW = next.mNative.mLinear[3];
            next.mOwned->mNativeAngularW = next.mNative.mAngular[3];
        }
    }

    void ActorRagdollPhysics::applyNativeVelocityStep(float frameSeconds,
        std::span<const osg::Vec3f> nativeLinearDeltas)
    {
        std::vector<std::array<float, 4>> packed;
        packed.reserve(nativeLinearDeltas.size());
        for (const auto& delta : nativeLinearDeltas)
            packed.push_back({delta[0], delta[1], delta[2], 0.f});
        applyNativePackedVelocityStep(frameSeconds, packed);
    }

    void ActorRagdollPhysics::stepNativeKeyframedMotion(float frameSeconds)
    {
        coefficient(frameSeconds);
        struct Pending
        {
            Impl::Body* mOwned;
            btTransform mPose;
            btVector3 mLinear, mAngular;
            float mLinearW, mAngularW;
        };
        std::vector<Pending> pending;
        pending.reserve(mImpl->mBodies.size());
        for (auto& owned : mImpl->mBodies)
        {
            const auto& body = *owned.mBody;
            if (owned.mMotion != RagdollNativeMotion::Keyframed || !body.isActive())
                continue;
            const auto physical = body.getWorldTransform() * owned.mCenterFrame.inverse();
            const auto rotation = physical.getRotation();
            std::array<float, 4> quaternion;
            for (unsigned axis = 0; axis < 4; ++axis)
                quaternion[axis] = static_cast<float>(rotation[axis]);
            osg::Vec3f center, local;
            RagdollNativeVelocities velocity;
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                center[axis] = static_cast<float>(body.getCenterOfMassPosition()[axis] / mImpl->mLengthScale);
                local[axis] = static_cast<float>(owned.mCenterFrame.getOrigin()[axis] / mImpl->mLengthScale);
                velocity.mLinear[axis] = static_cast<float>(body.getLinearVelocity()[axis] / mImpl->mLengthScale);
                velocity.mAngular[axis] = static_cast<float>(body.getAngularVelocity()[axis]);
            }
            const auto step = ragdollNativeKeyframedMotionStep(center, quaternion, local,
                velocity, frameSeconds, owned.mLimits.mMaxLinearVelocity, owned.mLimits.mAngularLimit,
                owned.mNativeLinearW, owned.mNativeAngularW);
            const auto matrix = ragdollBoneWorldFromNativeBlendPose(step.mBodyPose);
            btMatrix3x3 basis;
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned col = 0; col < 3; ++col)
                    basis[row][col] = matrix(col, row);
            // Bullet stores the principal inertia frame at physical COM.
            // Rebuild that frame once; scene/bodyT offsets are already applied.
            const btTransform pose(basis * owned.mCenterFrame.getBasis(),
                vector(step.mCenterOfMass) * mImpl->mLengthScale);
            validatePose(pose);
            const auto linear = vector(step.mVelocities.mLinear) * mImpl->mLengthScale;
            const auto angular = vector(step.mVelocities.mAngular);
            require(finite(linear) && finite(angular), "keyframed velocity exceeds world domain");
            pending.push_back({&owned, pose, linear, angular, step.mLinearW, step.mAngularW});
        }
        for (const auto& change : pending)
        {
            auto& owned = *change.mOwned;
            auto& body = *owned.mBody;
            body.setLinearVelocity(change.mLinear);
            body.setAngularVelocity(change.mAngular);
            owned.mNativeLinearW = change.mLinearW;
            owned.mNativeAngularW = change.mAngularW;
            // For kinematic bodies Bullet retains the previous transform here
            // and copies our capped velocities into interpolation state.
            body.setCenterOfMassTransform(change.mPose);
            if (mImpl->mPublished && mImpl->worldAlive()) mImpl->mWorld.updateSingleAabb(&body);
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
            const auto sceneRotation = ragdollNativeSceneTargetFromBodyPose({{}, quaternion}, found->mSceneOffset);
            currentCenter = ragdollNativeSceneCenterOfMass(currentCenter, quaternion, found->mSceneOffset);
            const auto target = ragdollNativeTargetVelocities(localCenter, currentCenter, sceneRotation.mRotation,
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
        auto states = captureNativePackedVelocities();
        std::vector<std::pair<btVector3, btVector3>> pending(states.size());
        for (std::size_t i = 0; i < states.size(); ++i)
        {
            const auto& owned = mImpl->mBodies[i];
            if (owned.mMotion != RagdollNativeMotion::Dynamic)
                continue;
            const auto damp = [&](std::array<float, 4>& velocity, float coefficient) {
                const float factor = float(std::max(0.0, 1.0 - double(frameSeconds) * coefficient));
                for (float& value : velocity)
                {
                    require(std::isfinite(value), "velocity exceeds native float domain");
                    value *= factor;
                }
            };
            damp(states[i].mVelocities.mLinear, owned.mLinearDamping);
            damp(states[i].mVelocities.mAngular, owned.mAngularDamping);
            pending[i] = packedWorldVelocity(states[i].mVelocities, mImpl->mLengthScale);
        }
        // Explicit damping also updates sleepers, without changing activation.
        for (std::size_t i = 0; i < states.size(); ++i)
        {
            auto& owned = mImpl->mBodies[i];
            if (owned.mMotion != RagdollNativeMotion::Dynamic)
                continue;
            owned.mBody->setLinearVelocity(pending[i].first);
            owned.mBody->setAngularVelocity(pending[i].second);
            owned.mNativeLinearW = states[i].mVelocities.mLinear[3];
            owned.mNativeAngularW = states[i].mVelocities.mAngular[3];
        }
    }
}
