#include "actorragdoll.hpp"

#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include <components/nif/node.hpp>
#include <components/nif/controller.hpp>
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

    RagdollBodyDefinition ragdollBodyWithNativeScaledProperties(
        const RagdollBodyDefinition& source, float resolvedActorScale)
    {
        positive(resolvedActorScale);
        const double scale = resolvedActorScale;
        // Original 8A2D60 stores the cube only after both x87 multiplies.
        const float inertiaScale = static_cast<float>(scale * scale * scale);
        positive(inertiaScale);
        const auto multiply = [](float value, double factor) {
            require(std::isfinite(value), "nonfinite scaled property input");
            const float result = static_cast<float>(double(value) * factor);
            require(std::isfinite(result), "nonfinite scaled property result");
            return result;
        };
        const auto vector = [&](const osg::Vec3f& value) {
            return osg::Vec3f(multiply(value[0], scale), multiply(value[1], scale),
                multiply(value[2], scale));
        };
        // Own the complete copy before changing any property. Caller state and
        // shared authored definitions survive rejected and successful scaling.
        RagdollBodyDefinition result = source;
        positive(source.mMass);
        result.mMass = multiply(source.mMass, scale);
        positive(result.mMass);
        result.mCenter = vector(source.mCenter);
        for (std::size_t i = 0; i < result.mInertia.size(); ++i)
            result.mInertia[i] = multiply(source.mInertia[i], inertiaScale);
        std::visit([&](auto& shape) {
            using Shape = std::decay_t<decltype(shape)>;
            if constexpr (std::is_same_v<Shape, RagdollSphere>)
            {
                positive(shape.mRadius);
                shape.mRadius = multiply(shape.mRadius, scale);
                positive(shape.mRadius);
            }
            else if constexpr (std::is_same_v<Shape, RagdollCapsule>)
            {
                positive(shape.mRadius1);
                positive(shape.mRadius2);
                shape.mPoint1 = vector(shape.mPoint1);
                shape.mPoint2 = vector(shape.mPoint2);
                shape.mRadius1 = multiply(shape.mRadius1, scale);
                shape.mRadius2 = multiply(shape.mRadius2, scale);
                positive(shape.mRadius1);
                positive(shape.mRadius2);
            }
            else
            {
                nonnegative(shape.mRadius);
                require(shape.mVertices.size() >= 4, "insufficient hull vertices");
                for (auto& vertex : shape.mVertices)
                    vertex = vector(vertex);
                // Original 8C8AA0 leaves convex-hull collision radius intact.
                // Its plane-normal/offset array is not retained by this graph.
            }
        }, result.mShape);
        return result;
    }

    ActorRagdollDefinition ragdollDefinitionWithNativeScaledProperties(
        const ActorRagdollDefinition& source, float resolvedActorScale)
    {
        positive(resolvedActorScale);
        const auto vector = [&](const osg::Vec3f& value) {
            osg::Vec3f result;
            for (unsigned i = 0; i < 3; ++i)
            {
                require(std::isfinite(value[i]), "nonfinite scaled graph coordinate");
                result[i] = static_cast<float>(double(value[i]) * double(resolvedActorScale));
                require(std::isfinite(result[i]), "nonfinite scaled graph result");
            }
            return result;
        };
        ActorRagdollDefinition result = source;
        for (auto& body : result.mBodies)
        {
            body = ragdollBodyWithNativeScaledProperties(body, resolvedActorScale);
            // Ordinary bhkRigidBody uses its target bone. Original 8B8E70
            // scales the separate translation only for bhkRigidBodyT.
            if (body.mUsesRigidBodyTransform)
                body.mTranslation = vector(body.mTranslation);
        }
        for (auto& joint : result.mJoints)
        {
            require(joint.mBodyA < result.mBodies.size() && joint.mBodyB < result.mBodies.size()
                && joint.mBodyA != joint.mBodyB, "invalid scaled joint endpoints");
            std::visit([&](auto& value) {
                value.mA.mPivot = vector(value.mA.mPivot);
                value.mB.mPivot = vector(value.mB.mPivot);
                // Native cone/hinge copy preserves directions, angular limits
                // and friction. Malleable copy dispatches the same nested
                // operation, retaining its separate tau/damping coefficients.
            }, joint.mJoint);
        }
        return result;
    }

    std::optional<RagdollRootBlendDefinition> loadActorRagdollRootBlend(
        Nif::FileView file, std::optional<std::uint32_t> rootRecord)
    {
        require((file.getVersion() == 0x14000004 || file.getVersion() == Nif::NIFFile::VER_OB)
            && file.getBethVersion() <= 16, "unsupported NIF version");
        if (!rootRecord)
            return std::nullopt;
        require(*rootRecord < file.numRecords(), "root record outside source file");
        const auto member = [&](const Nif::Record* record) {
            require(record && record->mRecordIndex < file.numRecords()
                && file.getRecord(record->mRecordIndex) == record, "reference outside source file");
        };
        const auto blendAt = [&](const Nif::NiAVObject* node) -> std::optional<RagdollRootBlendDefinition> {
            member(node);
            if (node->mCollision.empty())
                return std::nullopt;
            member(node->mCollision.getPtr());
            const auto* blend = dynamic_cast<const Nif::bhkBlendCollisionObject*>(node->mCollision.getPtr());
            if (!blend)
                return std::nullopt;
            require(!blend->mTarget.empty() && blend->mTarget.getPtr() == node && !blend->mBody.empty(),
                "invalid root blend target");
            member(blend->mBody.getPtr());
            const auto* body = dynamic_cast<const Nif::bhkRigidBody*>(blend->mBody.getPtr());
            require(body, "invalid root blend body");
            require(std::isfinite(blend->mHeirGain) && std::isfinite(blend->mVelGain),
                "nonfinite authored blend gain");
            return RagdollRootBlendDefinition{file.getHash(), node->mRecordIndex, body->mRecordIndex,
                {blend->mRecordIndex, blend->mFlags, blend->mHeirGain, blend->mVelGain}};
        };
        const auto childSlots = [](const Nif::NiNode* node) -> const Nif::NiAVObjectList& {
            require(node && node->mChildren.size() <= 0xffff, "invalid native child array");
            return node->mChildren;
        };

        const auto* root = dynamic_cast<const Nif::NiAVObject*>(file.getRecord(*rootRecord));
        if (auto value = blendAt(root))
            return value;
        const auto& roots = childSlots(dynamic_cast<const Nif::NiNode*>(root));
        // The original dereferences the first slot unconditionally after its
        // extent check. Diagnose malformed absent/null first children safely.
        require(!roots.empty() && !roots.front().empty(), "missing first root child");
        const auto* first = roots.front().getPtr();
        if (auto value = blendAt(first))
            return value;
        const auto& children = childSlots(dynamic_cast<const Nif::NiNode*>(first));
        std::size_t nonnull = 0;
        for (const auto& child : children)
            nonnull += !child.empty();
        const std::size_t selected = nonnull == 1 ? 0 : 1;
        if (selected >= children.size() || children[selected].empty())
            return std::nullopt;
        const auto* node = children[selected].getPtr();
        if (auto value = blendAt(node))
            return value;
        if (const auto* branch = dynamic_cast<const Nif::NiNode*>(node))
            for (const auto& child : childSlots(branch))
            {
                require(!child.empty(), "null root blend search child");
                if (auto value = blendAt(child.getPtr()))
                    return value;
            }
        return std::nullopt;
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
            value.mUsesRigidBodyTransform = body->mRecordType == Nif::RC_bhkRigidBodyT;
            if (const auto* blend = dynamic_cast<const Nif::bhkBlendCollisionObject*>(node->mCollision.getPtr()))
            {
                require(std::isfinite(blend->mHeirGain) && std::isfinite(blend->mVelGain),
                    "nonfinite authored blend gain");
                value.mBlend = RagdollBlendDefinition{blend->mRecordIndex, blend->mFlags,
                    blend->mHeirGain, blend->mVelGain};
                // Original700010 searches the attached chain in order and
                // returns at the first matching RTTI. Its target is metadata,
                // not a selection predicate; later controllers are unused.
                std::unordered_set<const Nif::NiTimeController*> controllers;
                for (const auto* current = node->mController.empty() ? nullptr : node->mController.getPtr(); current;
                     current = current->mNext.empty() ? nullptr : current->mNext.getPtr())
                {
                    member(current);
                    require(controllers.insert(current).second, "cyclic native blend controller lookup");
                    if (const auto* controller = dynamic_cast<const Nif::bhkBlendController*>(current))
                    {
                        std::optional<std::uint32_t> target;
                        if (!controller->mTarget.empty())
                        {
                            member(controller->mTarget.getPtr());
                            target = controller->mTarget->mRecordIndex;
                        }
                        value.mBlendController = RagdollBlendControllerDefinition{controller->mRecordIndex,
                            target, controller->mFlags, controller->mFrequency, controller->mPhase,
                            controller->mTimeStart, controller->mTimeStop, {}};
                        auto& keys = value.mBlendController->mKeys;
                        keys.reserve(controller->mKeys.size());
                        for (const auto& key : controller->mKeys)
                            keys.push_back({key.mTime, key.mHierarchyGain, key.mVelocityGain});
                        break;
                    }
                }
            }
            const auto& info = body->mInfo;
            value.mWorldObjectFilter = { body->mHavokFilter.mLayer, body->mHavokFilter.mFlags,
                body->mHavokFilter.mGroup };
            value.mInfoFilter = { info.mHavokFilter.mLayer, info.mHavokFilter.mFlags,
                info.mHavokFilter.mGroup };
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
