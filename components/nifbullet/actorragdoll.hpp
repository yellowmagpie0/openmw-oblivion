#ifndef OPENMW_COMPONENTS_NIFBULLET_ACTORRAGDOLL_HPP
#define OPENMW_COMPONENTS_NIFBULLET_ACTORRAGDOLL_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <osg/Matrixf>
#include <osg/Quat>
#include <osg/Vec3f>

#include <components/nif/niffile.hpp>

namespace NifBullet
{
    // An owned description, independent of NIF pointer lifetime and any live
    // Bullet world. Shape/body/joint coordinates retain their Havok units;
    // the target node's bind matrix uses the skeleton's NetImmerse units.
    struct RagdollSphere { float mRadius; };
    struct RagdollCapsule
    {
        osg::Vec3f mPoint1, mPoint2;
        float mRadius1, mRadius2;
    };
    struct RagdollHull
    {
        std::vector<osg::Vec3f> mVertices;
        float mRadius;
    };
    struct RagdollBox
    {
        // Native hkBoxShape stores core half extents, expanded by radius.
        osg::Vec3f mHalfExtents;
        float mRadius;
    };
    struct RagdollBodyFilter
    {
        std::uint8_t mLayer = 0, mFlags = 0;
        std::uint16_t mGroup = 0;
    };
    struct RagdollShape;
    struct RagdollList
    {
        std::vector<RagdollShape> mChildren;
        // Preserve native collection metadata; collision policy belongs to the caller.
        std::vector<RagdollBodyFilter> mFilters;
    };
    struct RagdollTransform
    {
        osg::Matrixf mTransform;
        // Wrapper metadata; the child supplies the convex collision radius.
        float mRadius;
        // Exactly one child. A vector permits a deeply owned recursive value.
        std::vector<RagdollShape> mChildren;
    };
    using RagdollShapeValue = std::variant<RagdollSphere, RagdollCapsule, RagdollHull,
        RagdollBox, RagdollList, RagdollTransform>;
    struct RagdollShape : RagdollShapeValue
    {
        using RagdollShapeValue::RagdollShapeValue;
        using RagdollShapeValue::operator=;
    };

    struct RagdollBlendDefinition
    {
        std::uint32_t mRecord;
        std::uint16_t mFlags;
        float mHierarchyGain, mVelocityGain;
    };

    struct RagdollRootBlendDefinition
    {
        std::string mSourceHash;
        std::uint32_t mNodeRecord, mBodyRecord;
        RagdollBlendDefinition mBlend;
    };

    // Select from the explicitly supplied scene root using native child-slot
    // order. File root order and rigid-body record order are not substitutes.
    // The result owns metadata; it does not advance live blend state.
    std::optional<RagdollRootBlendDefinition> loadActorRagdollRootBlend(
        Nif::FileView file, std::optional<std::uint32_t> rootRecord);

    struct RagdollBlendControllerKey
    {
        float mTime, mHierarchyGain, mVelocityGain;
    };

    // Owned authored timing/identity/keys from the first matching controller.
    // No runtime clock caches, attachment or simulation are inferred.
    struct RagdollBlendControllerDefinition
    {
        std::uint32_t mRecord;
        std::optional<std::uint32_t> mTargetRecord;
        std::uint16_t mFlags;
        float mFrequency, mPhase, mStartTime, mStopTime;
        std::vector<RagdollBlendControllerKey> mKeys;
    };

    struct RagdollBodyDefinition
    {
        std::uint32_t mRecord;
        std::uint32_t mNodeRecord;
        std::string mBone;
        osg::Matrixf mBoneBind;
        bool mUsesRigidBodyTransform = false;
        // Authored collision-object flags, separate from both Havok filters.
        // Bit0 controls ordinary physics-to-scene dispatch after registration.
        std::uint16_t mCollisionFlags = 1;
        RagdollBodyFilter mWorldObjectFilter, mInfoFilter;
        // Authored bhkBlendCollisionObject data, not live blend progression.
        std::optional<RagdollBlendDefinition> mBlend;
        std::optional<RagdollBlendControllerDefinition> mBlendController;
        osg::Vec3f mTranslation;
        osg::Quat mRotation;
        osg::Vec3f mCenter;
        std::array<float, 9> mInertia;
        float mMass;
        float mLinearDamping, mAngularDamping;
        float mFriction, mRestitution;
        float mMaxLinearVelocity, mMaxAngularVelocity;
        RagdollShape mShape;
    };

    // Scale cached mass, inertia, center and supported shape properties only.
    // Does not scale bone/body/joint transforms or admit a scaled live actor.
    RagdollBodyDefinition ragdollBodyWithNativeScaledProperties(
        const RagdollBodyDefinition& source, float resolvedActorScale);

    struct RagdollJointFrame
    {
        osg::Vec3f mPivot;
        osg::Vec3f mAxis;
        osg::Vec3f mPlane;
    };
    struct RagdollConeJoint
    {
        RagdollJointFrame mA, mB;
        float mConeAngle;
        float mPlaneMin, mPlaneMax;
        float mTwistMin, mTwistMax;
        float mFriction;
    };
    struct RagdollHingeJoint
    {
        RagdollJointFrame mA, mB;
        float mMin, mMax;
        float mFriction;
    };
    using RagdollJoint = std::variant<RagdollConeJoint, RagdollHingeJoint>;
    struct RagdollJointDefinition
    {
        std::uint32_t mRecord;
        std::size_t mBodyA, mBodyB;
        bool mMalleable = false;
        float mTau = 1;
        float mDamping = 1;
        RagdollJoint mJoint;
    };
    struct ActorRagdollDefinition
    {
        std::string mSourceHash;
        std::vector<RagdollBodyDefinition> mBodies;
        std::vector<RagdollJointDefinition> mJoints;
        // Present only when loaded for a new placed reference. Its authored
        // root transform is omitted from bind matrices and replaced in 3D.
        std::optional<std::uint32_t> mReferenceRoot;
        // The original child remains visible after its collision is relocated.
        // Retain its identity for model admission without treating it as a body.
        struct ReferenceCollisionSource
        {
            std::uint32_t mNodeRecord;
            std::string mBone;
            // Preserve the pre-relocation inputs. Native cloning scales bodyT
            // properties before registration combines them with unchanged Ni
            // child R/T. Scaling the combined offset would also scale that R/T.
            std::uint32_t mBodyRecord;
            osg::Matrixf mNodeLocal;
            bool mUsesRigidBodyTransform;
            osg::Vec3f mBodyTranslation;
            osg::Quat mBodyRotation;
        };
        std::optional<ReferenceCollisionSource> mReferenceCollisionSource;
    };

    // Own a native property-scaled graph, including bodyT translation and
    // joint pivots. Bone bind matrices remain authored; renderer placement
    // supplies actor scale separately. Does not admit a scaled live binding.
    ActorRagdollDefinition ragdollDefinitionWithNativeScaledProperties(
        const ActorRagdollDefinition& source, float resolvedActorScale);

    // The currently admitted Oblivion skeleton graph uses sphere/capsule/hull
    // bodies, box/list/transformed weapon shapes and ragdoll/limited-hinge joints, including malleable wrappers.
    // Unsupported or malformed graphs are diagnosed as a whole. Shape trees
    // are bounded to 128 edges and 4096 expanded instances per body.
    ActorRagdollDefinition loadActorRagdollDefinition(Nif::FileView file);
    // GenerateNiNode replaces the single asset root's local pose with the
    // reference placement. Registration relocates a selected dynamic child
    // collision to that root with a bodyT offset; visible child locals and
    // body properties/joint frames remain authored.
    ActorRagdollDefinition loadReferenceRagdollDefinition(Nif::FileView file);
}

#endif
