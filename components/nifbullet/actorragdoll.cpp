#include "actorragdoll.hpp"

#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include <components/nif/node.hpp>
#include <components/nif/physics.hpp>

namespace NifBullet
{
    namespace
    {
        void require(bool condition, const char* message)
        {
            if (!condition)
                throw std::runtime_error(std::string("Invalid actor ragdoll: ") + message);
        }

        template<class V> osg::Vec3f xyz(const V& value)
        {
            osg::Vec3f result(value[0], value[1], value[2]);
            require(std::isfinite(result.x()) && std::isfinite(result.y()) && std::isfinite(result.z()),
                "nonfinite vector");
            return result;
        }

        void nonnegative(float value)
        {
            require(std::isfinite(value) && value >= 0, "invalid nonnegative coefficient");
        }
        void positive(float value)
        {
            require(std::isfinite(value) && value > 0, "invalid positive coefficient");
        }
        void limits(float low, float high)
        {
            require(std::isfinite(low) && std::isfinite(high) && low <= high, "invalid joint limits");
        }
        RagdollJointFrame frame(const osg::Vec4f& pivot, const osg::Vec4f& axis, const osg::Vec4f& plane)
        {
            RagdollJointFrame result{ xyz(pivot), xyz(axis), xyz(plane) };
            require(result.mAxis.length2() > 0 && result.mPlane.length2() > 0
                && (result.mAxis ^ result.mPlane).length2() > 0, "degenerate joint frame");
            return result;
        }
        RagdollConeJoint cone(const Nif::bhkRagdollConstraintCInfo& value)
        {
            nonnegative(value.mConeMaxAngle);
            limits(value.mPlaneMinAngle, value.mPlaneMaxAngle);
            limits(value.mTwistMinAngle, value.mTwistMaxAngle);
            nonnegative(value.mMaxFriction);
            // Oblivion does not serialize the later motor fields.
            return { frame(value.mDataA.mPivot, value.mDataA.mTwist, value.mDataA.mPlane),
                frame(value.mDataB.mPivot, value.mDataB.mTwist, value.mDataB.mPlane),
                value.mConeMaxAngle, value.mPlaneMinAngle, value.mPlaneMaxAngle,
                value.mTwistMinAngle, value.mTwistMaxAngle, value.mMaxFriction };
        }
        RagdollHingeJoint hinge(const Nif::bhkLimitedHingeConstraintCInfo& value)
        {
            limits(value.mMinAngle, value.mMaxAngle);
            nonnegative(value.mMaxFriction);
            return { frame(value.mDataA.mPivot, value.mDataA.mAxis, value.mDataA.mPerpAxis1),
                // The Oblivion layout stores B's second perpendicular axis only.
                // Reconstruct the first; never read the uninitialized later field.
                frame(value.mDataB.mPivot, value.mDataB.mAxis,
                    osg::Vec4f(xyz(value.mDataB.mPerpAxis2) ^ xyz(value.mDataB.mAxis), 0)),
                value.mMinAngle, value.mMaxAngle, value.mMaxFriction };
        }
    }

    ActorRagdollDefinition loadActorRagdollDefinition(Nif::FileView file)
    {
        require((file.getVersion() == 0x14000004 || file.getVersion() == Nif::NIFFile::VER_OB)
            && file.getBethVersion() <= 16, "unsupported NIF version");
        ActorRagdollDefinition result;
        result.mSourceHash = file.getHash();
        std::unordered_set<const Nif::Record*> records;
        for (std::size_t i = 0; i < file.numRecords(); ++i)
        {
            const auto* record = file.getRecord(i);
            require(record && record->mRecordIndex == i && records.insert(record).second,
                "invalid record membership");
        }
        const auto member = [&](const Nif::Record* record) {
            require(record && records.contains(record), "reference outside source file");
        };

        // Traverse iteratively: corrupt cycles or extremely deep hierarchies must
        // not overflow the stack. A bone must have one unambiguous bind matrix.
        struct Pending { const Nif::NiAVObject* mNode; osg::Matrixf mParent; };
        std::vector<Pending> pending;
        for (std::size_t i = 0; i < file.numRoots(); ++i)
        {
            member(file.getRoot(i));
            if (const auto* node = dynamic_cast<const Nif::NiAVObject*>(file.getRoot(i)))
                pending.push_back({ node, osg::Matrixf::identity() });
        }
        std::unordered_map<const Nif::NiAVObject*, osg::Matrixf> bind;
        std::unordered_map<const Nif::bhkRigidBody*, const Nif::NiAVObject*> targets;
        while (!pending.empty())
        {
            const auto [node, parent] = pending.back();
            pending.pop_back();
            member(node);
            const auto matrix = node->mTransform.toMatrix() * parent;
            for (unsigned i = 0; i < 16; ++i)
                require(std::isfinite(matrix.ptr()[i]), "nonfinite bind matrix");
            require(bind.emplace(node, matrix).second, "cyclic or ambiguous bone hierarchy");
            if (!node->mCollision.empty())
            {
                member(node->mCollision.getPtr());
                const auto* collision = dynamic_cast<const Nif::bhkCollisionObject*>(node->mCollision.getPtr());
                require(collision && !collision->mTarget.empty() && collision->mTarget.getPtr() == node
                    && !collision->mBody.empty(), "invalid collision target");
                member(collision->mBody.getPtr());
                const auto* body = dynamic_cast<const Nif::bhkRigidBody*>(collision->mBody.getPtr());
                require(body && targets.emplace(body, node).second, "invalid or duplicate collision body");
            }
            if (const auto* branch = dynamic_cast<const Nif::NiNode*>(node))
                for (const auto& child : branch->mChildren)
                    if (!child.empty())
                        pending.push_back({ child.getPtr(), matrix });
        }
        std::unordered_map<const Nif::bhkRigidBody*, std::size_t> bodies;
        std::unordered_set<std::string> names;
        for (std::size_t i = 0; i < file.numRecords(); ++i)
        {
            const auto* body = dynamic_cast<const Nif::bhkRigidBody*>(file.getRecord(i));
            if (!body)
                continue;
            require(targets.contains(body), "body has no reachable target");
            const auto* node = targets.at(body);
            require(!node->mName.empty() && names.insert(node->mName).second, "missing or duplicate bone name");
            require(!body->mShape.empty(), "body has no shape");
            member(body->mShape.getPtr());
            RagdollBodyDefinition value;
            value.mRecord = body->mRecordIndex;
            value.mNodeRecord = node->mRecordIndex;
            value.mBone = node->mName;
            value.mBoneBind = bind.at(node);
            const auto& info = body->mInfo;
            value.mTranslation = xyz(info.mTranslation);
            value.mCenter = xyz(info.mCenter);
            value.mRotation = info.mRotation;
            for (unsigned j = 0; j < 4; ++j)
                require(std::isfinite(value.mRotation[j]), "nonfinite body rotation");
            require(value.mRotation.length2() > 0, "zero body rotation");
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned col = 0; col < 3; ++col)
                {
                    const float entry = info.mInertiaTensor.mValues[row][col];
                    require(std::isfinite(entry), "nonfinite inertia tensor");
                    value.mInertia[row * 3 + col] = entry;
                }
            positive(info.mMass);
            nonnegative(info.mLinearDamping);
            nonnegative(info.mAngularDamping);
            nonnegative(info.mFriction);
            nonnegative(info.mRestitution);
            positive(info.mMaxLinearVelocity);
            positive(info.mMaxAngularVelocity);
            value.mMass = info.mMass;
            value.mLinearDamping = info.mLinearDamping;
            value.mAngularDamping = info.mAngularDamping;
            value.mFriction = info.mFriction;
            value.mRestitution = info.mRestitution;
            value.mMaxLinearVelocity = info.mMaxLinearVelocity;
            value.mMaxAngularVelocity = info.mMaxAngularVelocity;
            const auto* shape = body->mShape.getPtr();
            if (shape->mRecordType == Nif::RC_bhkSphereShape)
            {
                const auto* sphere = dynamic_cast<const Nif::bhkSphereShape*>(shape);
                require(sphere, "invalid sphere record");
                positive(sphere->mRadius);
                value.mShape = RagdollSphere{ sphere->mRadius };
            }
            else if (const auto* capsule = dynamic_cast<const Nif::bhkCapsuleShape*>(shape))
            {
                positive(capsule->mRadius1);
                positive(capsule->mRadius2);
                value.mShape = RagdollCapsule{ xyz(capsule->mPoint1), xyz(capsule->mPoint2),
                    capsule->mRadius1, capsule->mRadius2 };
            }
            else if (const auto* hull = dynamic_cast<const Nif::bhkConvexVerticesShape*>(shape))
            {
                nonnegative(hull->mRadius);
                require(hull->mVertices.size() >= 4, "insufficient hull vertices");
                RagdollHull output{ {}, hull->mRadius };
                for (const auto& vertex : hull->mVertices)
                    output.mVertices.push_back(xyz(vertex));
                value.mShape = std::move(output);
            }
            else
                require(false, "unsupported body shape");
            bodies.emplace(body, result.mBodies.size());
            result.mBodies.push_back(std::move(value));
        }

        std::unordered_set<const Nif::bhkConstraint*> referenced;
        for (const auto& entry : bodies)
        {
            const auto* body = entry.first;
            for (const auto& reference : body->mConstraints)
            {
                require(!reference.empty(), "null constraint reference");
                member(reference.getPtr());
                const auto* joint = dynamic_cast<const Nif::bhkConstraint*>(reference.getPtr());
                require(joint, "unsupported constraint reference");
                require(!joint->mInfo.mEntityA.empty() && !joint->mInfo.mEntityB.empty()
                    && (joint->mInfo.mEntityA.getPtr() == body || joint->mInfo.mEntityB.getPtr() == body),
                    "constraint attached to unrelated body");
                referenced.insert(joint);
            }
        }
        // Source order makes the owned representation deterministic, independent
        // of unordered traversal and constraints listed by both bodies.
        for (std::size_t i = 0; i < file.numRecords(); ++i)
        {
            const auto* joint = dynamic_cast<const Nif::bhkConstraint*>(file.getRecord(i));
            if (!joint)
                continue;
            require(referenced.contains(joint), "unreferenced constraint");
            require(!joint->mInfo.mEntityA.empty() && !joint->mInfo.mEntityB.empty(), "missing constraint endpoint");
            const auto* a = dynamic_cast<const Nif::bhkRigidBody*>(joint->mInfo.mEntityA.getPtr());
            const auto* b = dynamic_cast<const Nif::bhkRigidBody*>(joint->mInfo.mEntityB.getPtr());
            require(a && b && a != b && bodies.contains(a) && bodies.contains(b), "invalid constraint endpoints");
            RagdollJointDefinition value;
            value.mRecord = joint->mRecordIndex;
            value.mBodyA = bodies.at(a);
            value.mBodyB = bodies.at(b);
            if (const auto* ragdoll = dynamic_cast<const Nif::bhkRagdollConstraint*>(joint))
                value.mJoint = cone(ragdoll->mConstraint);
            else if (const auto* limited = dynamic_cast<const Nif::bhkLimitedHingeConstraint*>(joint))
                value.mJoint = hinge(limited->mConstraint);
            else if (const auto* malleable = dynamic_cast<const Nif::bhkMalleableConstraint*>(joint))
            {
                const auto& wrapped = malleable->mConstraint;
                // Stock Oblivion wrappers leave both inner endpoints empty.
                // A populated pair must agree with the authoritative outer pair.
                require((wrapped.mInfo.mEntityA.empty() && wrapped.mInfo.mEntityB.empty())
                    || (!wrapped.mInfo.mEntityA.empty() && !wrapped.mInfo.mEntityB.empty()
                        && wrapped.mInfo.mEntityA.getPtr() == a && wrapped.mInfo.mEntityB.getPtr() == b),
                    "malleable endpoint disagreement");
                value.mMalleable = true;
                nonnegative(wrapped.mTau);
                nonnegative(wrapped.mDamping);
                value.mTau = wrapped.mTau;
                value.mDamping = wrapped.mDamping;
                if (wrapped.mType == Nif::HkConstraintType::Ragdoll)
                    value.mJoint = cone(wrapped.mRagdollInfo);
                else if (wrapped.mType == Nif::HkConstraintType::LimitedHinge)
                    value.mJoint = hinge(wrapped.mLimitedHingeInfo);
                else
                    require(false, "unsupported malleable joint");
            }
            else
                require(false, "unsupported joint");
            result.mJoints.push_back(std::move(value));
        }
        return result;
    }
}
