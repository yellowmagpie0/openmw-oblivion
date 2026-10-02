#include <components/nifbullet/ragdollbonepose.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace
{
    Nif::NiTransform pose(osg::Vec3f translation = {}, float scale = 1)
    {
        return { Nif::Matrix3(), translation, scale };
    }

    TEST(RagdollBoneWriteback, WorldOnlyKeepsLocalPoseAndBothScaleFields)
    {
        const auto local = pose({1, 2, 3}, .25f);
        const auto world = pose({4, 5, 6}, 3);
        const auto desired = pose({10, 20, 30}, 5);
        const auto result = NifBullet::ragdollBonePoseWriteback(local, world, desired, nullptr, 1, false);
        EXPECT_EQ(result.mLocal.mTranslation, local.mTranslation);
        EXPECT_FLOAT_EQ(result.mLocal.mScale, local.mScale);
        EXPECT_EQ(result.mWorld.mTranslation, desired.mTranslation);
        EXPECT_FLOAT_EQ(result.mWorld.mScale, world.mScale);
        EXPECT_EQ(result.mWorldChangeMask, 1);
    }

    TEST(RagdollBoneWriteback, LocalPoseUsesParentRotationTranslationAndSeparateScale)
    {
        auto parent = pose({10, 20, 30}, 2);
        parent.mRotation.mValues[0][0] = parent.mRotation.mValues[1][1] = 0;
        parent.mRotation.mValues[0][1] = -1;
        parent.mRotation.mValues[1][0] = 1;
        auto desired = pose({14, 26, 38}, 5);
        desired.mRotation.mValues[0][0] = desired.mRotation.mValues[1][1] = -1;
        const auto result = NifBullet::ragdollBonePoseWriteback(pose({}, .25f), pose({}, 3),
            desired, &parent, 9, false);
        EXPECT_EQ(result.mLocal.mTranslation, osg::Vec3f(3, -2, 4));
        EXPECT_EQ(result.mLocal.mRotation.mValues[0][0], 0);
        EXPECT_EQ(result.mLocal.mRotation.mValues[0][1], -1);
        EXPECT_EQ(result.mLocal.mRotation.mValues[1][0], 1);
        EXPECT_EQ(result.mLocal.mRotation.mValues[1][1], 0);
        EXPECT_FLOAT_EQ(result.mLocal.mScale, .25f);
        EXPECT_FLOAT_EQ(result.mWorld.mScale, 3);
        EXPECT_EQ(result.mWorldChangeMask, 3);
    }

    TEST(RagdollBoneWriteback, RootLocalWritePreservesScaleAndDoesNotRequireAParent)
    {
        const auto desired = pose({10, -20, 30}, 5);
        const auto result = NifBullet::ragdollBonePoseWriteback(pose({}, .25f), pose({}, 3),
            desired, nullptr, 9, false);
        EXPECT_EQ(result.mLocal.mTranslation, desired.mTranslation);
        EXPECT_FLOAT_EQ(result.mLocal.mScale, .25f);
        EXPECT_FLOAT_EQ(result.mWorld.mScale, 3);
    }

    TEST(RagdollBoneWriteback, SubthresholdWorldChangeStillWritesRequestedLocalPose)
    {
        const auto desired = pose({.005f, 0, 0});
        const auto result = NifBullet::ragdollBonePoseWriteback(pose(), pose(), desired, nullptr, 9, false);
        EXPECT_EQ(result.mLocal.mTranslation, desired.mTranslation);
        EXPECT_EQ(result.mWorld.mTranslation, osg::Vec3f());
        EXPECT_EQ(result.mWorldChangeMask, 0);
    }

    TEST(RagdollBoneWriteback, PublicationThresholdsAreInclusiveWithIndependentChangeBits)
    {
        auto desired = pose({.01f, 0, 0});
        desired.mRotation.mValues[0][1] = .001f;
        auto result = NifBullet::ragdollBonePoseWriteback(pose(), pose(), desired, nullptr, 1, false);
        EXPECT_EQ(result.mWorldChangeMask, 0);
        desired.mRotation.mValues[0][1] = std::nextafter(.001f, 1.f);
        result = NifBullet::ragdollBonePoseWriteback(pose(), pose(), desired, nullptr, 1, false);
        EXPECT_EQ(result.mWorldChangeMask, 2);
        // Any published change copies the complete rotation and position.
        EXPECT_FLOAT_EQ(result.mWorld.mTranslation.x(), .01f);
        desired = pose({std::nextafter(.01f, 1.f), 0, 0});
        result = NifBullet::ragdollBonePoseWriteback(pose(), pose(), desired, nullptr, 1, false);
        EXPECT_EQ(result.mWorldChangeMask, 1);
    }

    TEST(RagdollBoneWriteback, ForcedPublicationReportsBothBitsWithoutChangingScale)
    {
        const auto result = NifBullet::ragdollBonePoseWriteback(pose({}, .25f), pose({}, 3),
            pose({}, 5), nullptr, 1, true);
        EXPECT_EQ(result.mWorldChangeMask, 3);
        EXPECT_FLOAT_EQ(result.mLocal.mScale, .25f);
        EXPECT_FLOAT_EQ(result.mWorld.mScale, 3);
    }

    TEST(RagdollBoneWriteback, RejectsMalformedReadTransformsAndNonfiniteOutput)
    {
        auto invalid = pose();
        invalid.mRotation.mValues[1][2] = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(NifBullet::ragdollBonePoseWriteback(pose(), pose(), invalid, nullptr, 1, false),
            std::invalid_argument);
        invalid = pose({}, 0);
        EXPECT_THROW(NifBullet::ragdollBonePoseWriteback(pose(), pose(), pose(), &invalid, 9, false),
            std::invalid_argument);
        // A parent is not read on the world-only path.
        EXPECT_NO_THROW(NifBullet::ragdollBonePoseWriteback(pose(), pose(), pose(), &invalid, 1, false));
        invalid = pose({std::numeric_limits<float>::max(), 0, 0}, .01f);
        EXPECT_THROW(NifBullet::ragdollBonePoseWriteback(pose(), pose(), pose(), &invalid, 9, false),
            std::invalid_argument);
    }
}
