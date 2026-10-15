#include <apps/openmw/mwrender/obliviondropplacement.hpp>
#include <components/nifosg/matrixtransform.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>

#include <gtest/gtest.h>
#include <limits>
#include <utility>

namespace
{
    struct NativeDropScene
    {
        osg::ref_ptr<SceneUtil::PositionAttitudeTransform> mActor = new SceneUtil::PositionAttitudeTransform;
        osg::ref_ptr<NifOsg::MatrixTransform> mBone;
        osg::ref_ptr<osg::MatrixTransform> mModel = new osg::MatrixTransform;
        Nif::NiTransform mPlacement = Nif::NiTransform::getIdentity();
        NativeDropScene()
        {
            mActor->setPosition({12.25f, -9.5f, 1001.125f});
            mActor->setScale({2,2,2});
            mPlacement.mTranslation = mActor->getPosition(); mPlacement.mScale = 2;
            auto local = Nif::NiTransform::getIdentity();
            local.mRotation.mValues[0][0] = 2; // Retain embedded scale, independently of mScale.
            local.mTranslation = {1.25f,-2.5f,3.75f}; local.mScale = .25f;
            mBone = new NifOsg::MatrixTransform(local); mBone->setName("Weapon");
            mModel->setMatrix(osg::Matrix::translate(100,200,300));
            mActor->addChild(mBone); mBone->addChild(mModel);
        }
        auto capture() const
        {
            auto scene = MWRender::OblivionAttachedWeaponPose::capture(*mActor, *mBone);
            return scene ? MWRender::OblivionDroppedWeaponPose::capture(*scene, mPlacement) : std::nullopt;
        }
    };

    TEST(OblivionNativeDropPoseTest, UsesAnchorAndRetainsSeparateNativeScale)
    {
        NativeDropScene fixture;
        auto captured = fixture.capture(); ASSERT_TRUE(captured);
        EXPECT_TRUE(captured->matchesCurrentScene(fixture.mPlacement));
        EXPECT_EQ(captured->getAnchor(), fixture.mBone.get());
        const auto& world = captured->getNativeWorld();
        EXPECT_EQ(world.mTranslation, osg::Vec3f(14.75f,-14.5f,1008.625f));
        EXPECT_EQ(world.mScale, .5f);
        EXPECT_EQ(world.mRotation.mValues[0][0], 2);
        EXPECT_EQ(world.mRotation.mValues[1][1], 1);
        EXPECT_EQ(captured->getDropPlacement().mPosition.asVec3(), world.mTranslation);
        // Authored model-local offsets are applied by the newly instantiated
        // loose model. They are not added to the native reference drop position.
        fixture.mModel->setMatrix(osg::Matrix::translate(-400,500,600));
        EXPECT_TRUE(captured->matchesCurrentScene(fixture.mPlacement));
        EXPECT_EQ(captured->getDropPlacement().mPosition.asVec3(), world.mTranslation);
        fixture.mBone->setScale(-.25f);
        EXPECT_FALSE(captured->matchesCurrentScene(fixture.mPlacement));
        auto negative = fixture.capture(); ASSERT_TRUE(negative);
        EXPECT_EQ(negative->getNativeWorld().mScale, -.5f);
        EXPECT_EQ(negative->getNativeWorld().mRotation.mValues[0][0], 2);
        EXPECT_EQ(negative->getDropPlacement().mPosition.asVec3(), world.mTranslation);
        fixture.mActor->setScale({0,0,0}); fixture.mPlacement.mScale = 0;
        auto zeroRoot = fixture.capture(); ASSERT_TRUE(zeroRoot);
        EXPECT_EQ(zeroRoot->getNativeWorld().mScale, 0);
        EXPECT_EQ(zeroRoot->getDropPlacement().mPosition.asVec3(), fixture.mPlacement.mTranslation);
    }

    TEST(OblivionNativeDropPoseTest, RevalidatesHiddenScaleFieldsAndRenderedProceduralRotation)
    {
        NativeDropScene fixture;
        auto captured = fixture.capture(); ASSERT_TRUE(captured);
        const auto matrix = fixture.mBone->getMatrix();
        fixture.mBone->mScale = .5f; // Same OSG matrix, different native meaning.
        EXPECT_FALSE(captured->matchesCurrentScene(fixture.mPlacement));
        fixture.mBone->mScale = .25f;
        EXPECT_TRUE(captured->matchesCurrentScene(fixture.mPlacement));
        // Procedural controllers intentionally can leave the cached rotation
        // unchanged. Capture the current matrix with the retained separate scale.
        fixture.mBone->mRotationScale.mValues[0][0] = 99;
        EXPECT_TRUE(captured->matchesCurrentScene(fixture.mPlacement));
        auto procedural = matrix; procedural(0,0) = .75;
        fixture.mBone->setMatrix(procedural);
        EXPECT_FALSE(captured->matchesCurrentScene(fixture.mPlacement));
        auto replacement = fixture.capture(); ASSERT_TRUE(replacement);
        EXPECT_EQ(replacement->getNativeWorld().mRotation.mValues[0][0], 3);
        fixture.mBone->setMatrix(matrix);
        EXPECT_FALSE(replacement->matchesCurrentScene(fixture.mPlacement));
        EXPECT_TRUE(captured->matchesCurrentScene(fixture.mPlacement));
    }

    TEST(OblivionNativeDropPoseTest, RejectsUnsupportedSourceAndOverflowWithoutPublication)
    {
        NativeDropScene fixture;
        auto captured = fixture.capture(); ASSERT_TRUE(captured);
        for (float invalid : {0.f, std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::quiet_NaN()})
        {
            fixture.mBone->mScale = invalid;
            EXPECT_FALSE(captured->matchesCurrentScene(fixture.mPlacement));
            EXPECT_FALSE(fixture.capture());
        }
        fixture.mBone->mScale = .25f;
        const auto matrix = fixture.mBone->getMatrix();
        auto overflow = matrix; overflow(0,0) = std::numeric_limits<double>::max();
        fixture.mBone->setMatrix(overflow);
        EXPECT_FALSE(captured->matchesCurrentScene(fixture.mPlacement));
        EXPECT_FALSE(fixture.capture());
        auto perspective = matrix; perspective(0,3) = 1;
        fixture.mBone->setMatrix(perspective);
        EXPECT_FALSE(fixture.capture());
        fixture.mBone->setMatrix(matrix);
        fixture.mBone->setNodeMask(0);
        EXPECT_FALSE(captured->matchesCurrentScene(fixture.mPlacement));
        fixture.mBone->setNodeMask(~0u);
        fixture.mActor->removeChild(fixture.mBone);
        EXPECT_FALSE(captured->matchesCurrentScene(fixture.mPlacement));
    }

    TEST(OblivionNativeDropPoseTest, OwnsSnapshotAndGuardsExplicitPlacementAcrossMoveAndCopy)
    {
        NativeDropScene fixture;
        auto captured = fixture.capture(); ASSERT_TRUE(captured);
        auto copied = *captured;
        auto moved = std::move(*captured);
        EXPECT_FALSE(captured->matchesCurrentScene(fixture.mPlacement));
        EXPECT_TRUE(copied.matchesCurrentScene(fixture.mPlacement));
        EXPECT_TRUE(moved.matchesCurrentScene(fixture.mPlacement));
        auto wrong = fixture.mPlacement; wrong.mTranslation.x() += 1;
        EXPECT_FALSE(copied.matchesCurrentScene(wrong));
        auto scene = MWRender::OblivionAttachedWeaponPose::capture(*fixture.mActor,*fixture.mBone);
        ASSERT_TRUE(scene);
        EXPECT_FALSE(MWRender::OblivionDroppedWeaponPose::capture(*scene,wrong));
        wrong = fixture.mPlacement; wrong.mScale = 1;
        EXPECT_FALSE(MWRender::OblivionDroppedWeaponPose::capture(*scene,wrong));
        fixture.mActor = nullptr; fixture.mBone = nullptr; fixture.mModel = nullptr;
        EXPECT_TRUE(copied.matchesCurrentScene(fixture.mPlacement));
        EXPECT_EQ(copied.getDropPlacement().mPosition.asVec3(), moved.getDropPlacement().mPosition.asVec3());
    }
}
