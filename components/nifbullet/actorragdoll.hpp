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
    using RagdollShape = std::variant<RagdollSphere, RagdollCapsule, RagdollHull>;

    struct RagdollBodyFilter
    {
        std::uint8_t mLayer = 0, mFlags = 0;
        std::uint16_t mGroup = 0;
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

    struct RagdollBodyDefinition
    {
        std::uint32_t mRecord;
        std::uint32_t mNodeRecord;
        std::string mBone;
        osg::Matrixf mBoneBind;
        bool mUsesRigidBodyTransform = false;
        RagdollBodyFilter mWorldObjectFilter, mInfoFilter;
        // Authored bhkBlendCollisionObject data, not live blend progression.
        std::optional<RagdollBlendDefinition> mBlend;
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
    };

    // The currently admitted Oblivion skeleton graph uses sphere/capsule/hull
    // bodies and ragdoll/limited-hinge joints, including malleable wrappers.
    // Unsupported or malformed graphs are diagnosed as a whole.
    ActorRagdollDefinition loadActorRagdollDefinition(Nif::FileView file);
}

#endif
