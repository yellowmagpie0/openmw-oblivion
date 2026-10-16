#include "actorragdoll.hpp"
#include "actorragdollphysics.hpp"

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
        constexpr std::size_t MaxShapeDepth = 128, MaxShapeNodes = 4096;

        void shapeBudget(const RagdollShape& source)
        {
            std::vector<std::pair<const RagdollShape*, std::size_t>> pending{{&source, 0}};
            std::size_t nodes = 0;
            while (!pending.empty())
            {
                const auto [shape, depth] = pending.back(); pending.pop_back();
                require(depth <= MaxShapeDepth && ++nodes <= MaxShapeNodes, "shape hierarchy exceeds limits");
                std::visit([&](const auto& value) {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<T, RagdollList> || std::is_same_v<T, RagdollTransform>)
                    {
                        if constexpr (std::is_same_v<T, RagdollTransform>)
                            require(value.mChildren.size() == 1, "transform requires exactly one child");
                        else
                            require(!value.mChildren.empty() && (value.mFilters.empty()
                                || value.mFilters.size() == value.mChildren.size()), "invalid list children/filters");
                        require(value.mChildren.size() <= MaxShapeNodes - nodes, "shape hierarchy exceeds limits");
                        for (const auto& child : value.mChildren) pending.emplace_back(&child, depth + 1);
                    }
                }, *shape);
            }
        }

        RagdollShape loadShape(const Nif::bhkShape* source,
            const std::unordered_set<const Nif::Record*>& members)
        {
            std::unordered_set<const Nif::Record*> path;
            std::size_t nodes = 0;
            const auto load = [&](auto&& self, const Nif::bhkShape* shape, std::size_t depth) -> RagdollShape {
                require(shape && members.contains(shape), "shape outside source file");
                require(depth <= MaxShapeDepth && ++nodes <= MaxShapeNodes, "shape hierarchy exceeds limits");
                require(path.insert(shape).second, "cyclic shape hierarchy");
                RagdollShape result;
                if (shape->mRecordType == Nif::RC_bhkSphereShape)
                {
                    const auto* value = dynamic_cast<const Nif::bhkSphereShape*>(shape);
                    require(value, "invalid sphere record"); positive(value->mRadius);
                    result = RagdollSphere{value->mRadius};
                }
                else if (const auto* capsule = dynamic_cast<const Nif::bhkCapsuleShape*>(shape))
                {
                    positive(capsule->mRadius1); positive(capsule->mRadius2);
                    result = RagdollCapsule{xyz(capsule->mPoint1), xyz(capsule->mPoint2), capsule->mRadius1, capsule->mRadius2};
                }
                else if (const auto* hull = dynamic_cast<const Nif::bhkConvexVerticesShape*>(shape))
                {
                    nonnegative(hull->mRadius); require(hull->mVertices.size() >= 4, "insufficient hull vertices");
                    RagdollHull output{{}, hull->mRadius};
                    for (const auto& vertex : hull->mVertices) output.mVertices.push_back(xyz(vertex));
                    result = std::move(output);
                }
                else if (const auto* box = dynamic_cast<const Nif::bhkBoxShape*>(shape))
                {
                    const auto extents = xyz(box->mExtents);
                    for (unsigned axis = 0; axis < 3; ++axis)
                        require(std::isfinite(extents[axis]) && extents[axis] >= 0
                            && double(extents[axis]) + box->mRadius > 0, "invalid box half extents");
                    nonnegative(box->mRadius); result = RagdollBox{extents, box->mRadius};
                }
                else if (const auto* list = dynamic_cast<const Nif::bhkListShape*>(shape))
                {
                    require(!list->mSubshapes.empty() && list->mSubshapes.size() <= MaxShapeNodes - nodes
                        && (list->mHavokFilters.empty() || list->mHavokFilters.size() == list->mSubshapes.size()),
                        "invalid list children/filters");
                    RagdollList output;
                    for (const auto& child : list->mSubshapes)
                    {
                        require(!child.empty(), "null list child");
                        output.mChildren.push_back(self(self, child.getPtr(), depth + 1));
                    }
                    for (const auto& filter : list->mHavokFilters)
                        output.mFilters.push_back({filter.mLayer, filter.mFlags, filter.mGroup});
                    result = std::move(output);
                }
                else if (const auto* transformed = dynamic_cast<const Nif::bhkConvexTransformShape*>(shape))
                {
                    require(!transformed->mShape.empty(), "null transformed child");
                    for (unsigned i = 0; i < 16; ++i)
                        require(std::isfinite(transformed->mTransform.ptr()[i]), "nonfinite shape transform");
                    nonnegative(transformed->mRadius);
                    result = RagdollTransform{transformed->mTransform, transformed->mRadius,
                        {self(self, transformed->mShape.getPtr(), depth + 1)}};
                }
                else require(false, "unsupported body shape");
                path.erase(shape);
                return result;
            };
            return load(load, source, 0);
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
        shapeBudget(source.mShape);
        RagdollBodyDefinition result = source;
        positive(source.mMass);
        result.mMass = multiply(source.mMass, scale);
        positive(result.mMass);
        result.mCenter = vector(source.mCenter);
        for (std::size_t i = 0; i < result.mInertia.size(); ++i)
            result.mInertia[i] = multiply(source.mInertia[i], inertiaScale);
        const auto scaleShape = [&](auto&& self, RagdollShape& value) -> void {
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
                else if constexpr (std::is_same_v<Shape, RagdollBox>)
                {
                    nonnegative(shape.mRadius);
                    for (unsigned i = 0; i < 3; ++i)
                        require(std::isfinite(shape.mHalfExtents[i]) && shape.mHalfExtents[i] >= 0
                            && double(shape.mHalfExtents[i]) + shape.mRadius > 0, "invalid box half extents");
                    shape.mHalfExtents = vector(shape.mHalfExtents);
                    for (unsigned i = 0; i < 3; ++i)
                        require(std::isfinite(shape.mHalfExtents[i]) && shape.mHalfExtents[i] >= 0
                            && double(shape.mHalfExtents[i]) + shape.mRadius > 0, "invalid box half extents");
                    // Original 8B7F40 scales cached extents, retaining convex radius.
                }
                else if constexpr (std::is_same_v<Shape, RagdollList> || std::is_same_v<Shape, RagdollTransform>)
                {
                    if constexpr (std::is_same_v<Shape, RagdollTransform>)
                    {
                        nonnegative(shape.mRadius);
                        for (unsigned i = 0; i < 16; ++i)
                            require(std::isfinite(shape.mTransform.ptr()[i]), "nonfinite shape transform");
                        shape.mTransform.setTrans(vector(shape.mTransform.getTrans()));
                    }
                    for (auto& child : shape.mChildren) self(self, child);
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
            }, value);
        };
        scaleShape(scaleShape, result.mShape);
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

    static ActorRagdollDefinition loadRagdollDefinition(Nif::FileView file, bool referencePlacement)
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
        if (referencePlacement)
        {
            require(file.numRoots() == 1, "reference placement requires one asset root");
            const auto* root = dynamic_cast<const Nif::NiAVObject*>(file.getRoot(0));
            member(root);
            result.mReferenceRoot = root->mRecordIndex;
        }

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
            const auto matrix = result.mReferenceRoot == node->mRecordIndex
                ? parent : node->mTransform.toMatrix() * parent;
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
            value.mShape = loadShape(body->mShape.getPtr(), records);
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
        if (referencePlacement)
        {
            const auto* root = static_cast<const Nif::NiAVObject*>(file.getRoot(0));
            const auto* selected = root;
            // Original4E1FD0 selects the root, literal first child, or literal
            // second child. It does not search recursively for a physical node.
            if (root->mCollision.empty())
                if (const auto* branch = dynamic_cast<const Nif::NiNode*>(root);
                    branch && !branch->mChildren.empty() && !branch->mChildren[0].empty())
                {
                    selected = branch->mChildren[0].getPtr();
                    if (selected->mCollision.empty() && branch->mChildren.size() > 1
                        && !branch->mChildren[1].empty())
                        selected = branch->mChildren[1].getPtr();
                }
            if (selected != root && !selected->mCollision.empty())
            {
                const auto* collision = static_cast<const Nif::bhkCollisionObject*>(selected->mCollision.getPtr());
                const auto* body = static_cast<const Nif::bhkRigidBody*>(collision->mBody.getPtr());
                // Original88C740 leaves fixed/keyframed wrappers on the child.
                if (body->mInfo.mMotionType != Nif::HkMotionType::Motion_Keyframed
                    && body->mInfo.mMotionType != Nif::HkMotionType::Motion_Fixed)
                {
                    auto& value = result.mBodies[bodies.at(body)];
                    auto local = selected->mTransform.mRotation.toOsgMatrix();
                    local.setTrans(selected->mTransform.mTranslation);
                    const auto offset = ragdollNativeRelocatedBodyOffset(local, value);
                    result.mReferenceCollisionSource = ActorRagdollDefinition::ReferenceCollisionSource{
                        selected->mRecordIndex, selected->mName};
                    value.mUsesRigidBodyTransform = true;
                    value.mTranslation = offset.mPosition;
                    value.mRotation = osg::Quat(offset.mRotation[0], offset.mRotation[1],
                        offset.mRotation[2], offset.mRotation[3]);
                    value.mNodeRecord = root->mRecordIndex;
                    value.mBone = root->mName;
                    value.mBoneBind = osg::Matrixf::identity();
                }
            }
        }
        return result;
    }

    ActorRagdollDefinition loadActorRagdollDefinition(Nif::FileView file)
    {
        return loadRagdollDefinition(file, false);
    }

    ActorRagdollDefinition loadReferenceRagdollDefinition(Nif::FileView file)
    {
        return loadRagdollDefinition(file, true);
    }
}
