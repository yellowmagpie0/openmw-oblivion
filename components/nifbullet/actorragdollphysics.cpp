#include "actorragdollphysics.hpp"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <unordered_set>
#include <stdexcept>

#include <BulletCollision/CollisionShapes/btCompoundShape.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <BulletCollision/CollisionShapes/btMultiSphereShape.h>
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletDynamics/ConstraintSolver/btHingeConstraint.h>
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
    }

    struct ActorRagdollPhysics::Impl
    {
        struct Body
        {
            std::uint32_t mRecord;
            btTransform mCenterFrame;
            float mLinearDamping, mAngularDamping;
            std::unique_ptr<btRigidBody> mBody;
        };
        btDynamicsWorld& mWorld;
        float mLengthScale;
        // Bodies are destroyed before shapes; neither owns the other's storage.
        std::vector<std::unique_ptr<btCollisionShape>> mShapes;
        std::vector<Body> mBodies;
        std::vector<std::unique_ptr<btTypedConstraint>> mConstraints;
        std::size_t mRegisteredBodies = 0, mRegisteredConstraints = 0;

        Impl(btDynamicsWorld& world, float scale) : mWorld(world), mLengthScale(scale) {}
        ~Impl()
        {
            while (mRegisteredConstraints)
                mWorld.removeConstraint(mConstraints[--mRegisteredConstraints].get());
            while (mRegisteredBodies)
                mWorld.removeRigidBody(mBodies[--mRegisteredBodies].mBody.get());
        }
    };

    ActorRagdollPhysics::ActorRagdollPhysics(const ActorRagdollDefinition& definition, btDynamicsWorld& world,
        float lengthScale, std::span<const btTransform> bodyPoses, int collisionGroup, int collisionMask)
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
            mImpl->mShapes.push_back(std::move(shape));
            mImpl->mShapes.push_back(std::move(compound));
            mImpl->mBodies.push_back({input.mRecord, centerFrame, input.mLinearDamping,
                input.mAngularDamping, std::move(body)});
        }
        for (const auto& input : definition.mJoints)
        {
            require(input.mBodyA < mImpl->mBodies.size() && input.mBodyB < mImpl->mBodies.size()
                && input.mBodyA != input.mBodyB, "invalid joint endpoints");
            // These require dedicated native constraint rows and must not be
            // silently approximated by a symmetric Bullet cone or rigid hinge.
            require(!input.mMalleable, "malleable joint solver is not implemented");
            const auto* hinge = std::get_if<RagdollHingeJoint>(&input.mJoint);
            require(hinge, "native ragdoll cone solver is not implemented");
            require(hinge->mFriction == 0, "hinge friction solver is not implemented");
            require(std::isfinite(hinge->mMin) && std::isfinite(hinge->mMax)
                && hinge->mMin <= hinge->mMax, "invalid hinge limits");
            auto& a = mImpl->mBodies[input.mBodyA];
            auto& b = mImpl->mBodies[input.mBodyB];
            auto constraint = std::make_unique<btHingeConstraint>(*a.mBody, *b.mBody,
                a.mCenterFrame.inverse() * hingeFrame(hinge->mA, lengthScale),
                b.mCenterFrame.inverse() * hingeFrame(hinge->mB, lengthScale), true);
            constraint->setLimit(hinge->mMin, hinge->mMax);
            mImpl->mConstraints.push_back(std::move(constraint));
        }
        // Publish only after every body and admitted constraint was built.
        for (auto& body : mImpl->mBodies)
        {
            world.addRigidBody(body.mBody.get(), collisionGroup, collisionMask);
            ++mImpl->mRegisteredBodies;
        }
        for (auto& constraint : mImpl->mConstraints)
        {
            world.addConstraint(constraint.get(), true);
            ++mImpl->mRegisteredConstraints;
        }
    }

    ActorRagdollPhysics::~ActorRagdollPhysics() = default;

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
            body.setWorldTransform(pose);
            body.setInterpolationWorldTransform(pose);
            body.setLinearVelocity(states[i].mLinearVelocity);
            body.setAngularVelocity(states[i].mAngularVelocity);
            body.clearForces();
            body.activate(true);
            mImpl->mWorld.updateSingleAabb(&body);
        }
    }

    void ActorRagdollPhysics::applyImpulse(std::size_t body, const btVector3& impulse, const btVector3& worldPoint)
    {
        require(body < mImpl->mBodies.size() && finite(impulse) && finite(worldPoint), "invalid impulse");
        auto& target = *mImpl->mBodies[body].mBody;
        target.activate(true);
        target.applyImpulse(impulse, worldPoint - target.getCenterOfMassPosition());
    }

    void ActorRagdollPhysics::applyNativeDamping(float frameSeconds)
    {
        require(std::isfinite(frameSeconds) && frameSeconds >= 0, "invalid frame duration");
        auto states = capture();
        for (std::size_t i = 0; i < states.size(); ++i)
        {
            const auto damp = [&](btVector3& velocity, float coefficient) {
                const float factor = static_cast<float>(std::max(0.0, 1.0 - double(frameSeconds) * coefficient));
                for (int axis = 0; axis < 3; ++axis)
                {
                    const float native = static_cast<float>(velocity[axis] / mImpl->mLengthScale);
                    require(std::isfinite(native), "velocity exceeds native float domain");
                    const float result = native * factor;
                    velocity[axis] = btScalar(result) * mImpl->mLengthScale;
                }
            };
            damp(states[i].mLinearVelocity, mImpl->mBodies[i].mLinearDamping);
            damp(states[i].mAngularVelocity, mImpl->mBodies[i].mAngularDamping);
        }
        // Damping changes velocity only; retain the live contact/activation state.
        for (std::size_t i = 0; i < states.size(); ++i)
        {
            mImpl->mBodies[i].mBody->setLinearVelocity(states[i].mLinearVelocity);
            mImpl->mBodies[i].mBody->setAngularVelocity(states[i].mAngularVelocity);
        }
    }
}
