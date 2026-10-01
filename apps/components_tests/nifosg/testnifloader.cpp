#include "../nif/node.hpp"

#include <components/nif/node.hpp>
#include <components/nif/extra.hpp>
#include <components/sceneutil/keyframe.hpp>
#include <components/nif/property.hpp>
#include <components/nifosg/nifloader.hpp>
#include <components/nifosg/autotransform.hpp>
#include <components/nifosg/controller.hpp>
#include <components/nifosg/particle.hpp>
#include <components/resource/bgsmfilemanager.hpp>
#include <components/resource/imagemanager.hpp>
#include <components/sceneutil/serialize.hpp>
#include <components/vfs/manager.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <osgDB/Registry>

#include <osgParticle/ModularProgram>

#include <array>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using namespace testing;
    using namespace NifOsg;
    using namespace Nif::Testing;

    constexpr VFS::Path::NormalizedView testNif("test.nif");

    struct BaseNifOsgLoaderTest
    {
        VFS::Manager mVfs;
        Resource::ImageManager mImageManager{ &mVfs, 0 };
        Resource::BgsmFileManager mMaterialManager{ &mVfs, 0 };
        const osgDB::ReaderWriter* mReaderWriter = osgDB::Registry::instance()->getReaderWriterForExtension("osgt");
        osg::ref_ptr<osgDB::Options> mOptions = new osgDB::Options;

        BaseNifOsgLoaderTest()
        {
            SceneUtil::registerSerializers();

            if (mReaderWriter == nullptr)
                throw std::runtime_error("osgt reader writer is not found");

            mOptions->setPluginStringData("fileType", "Ascii");
            mOptions->setPluginStringData("WriteImageHint", "UseExternal");
        }

        std::string serialize(const osg::Node& node) const
        {
            std::stringstream stream;
            mReaderWriter->writeNode(node, stream, mOptions);
            std::string result;
            for (std::string line; std::getline(stream, line);)
            {
                if (line.starts_with('#'))
                    continue;
                line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);
                result += line;
                result += '\n';
            }
            return result;
        }
    };

    struct NifOsgLoaderTest : Test, BaseNifOsgLoaderTest
    {
    };

    TEST_F(NifOsgLoaderTest, shouldLoadFileWithDefaultNode)
    {
        Nif::NiAVObject node;
        init(node);
        Nif::NIFFile file(testNif);
        file.mRoots.push_back(&node);
        auto result = Loader::load(file, &mImageManager, &mMaterialManager);
        EXPECT_EQ(serialize(*result), R"(
osg::Group {
  UniqueID 1
  DataVariance STATIC
  UserDataContainer TRUE {
    osg::DefaultUserDataContainer {
      UniqueID 2
      UDC_UserObjects 1 {
        osg::StringValueObject {
          UniqueID 3
          Name "fileHash"
        }
      }
    }
  }
  Children 1 {
    osg::Group {
      UniqueID 4
      DataVariance STATIC
      UserDataContainer TRUE {
        osg::DefaultUserDataContainer {
          UniqueID 5
          UDC_UserObjects 1 {
            osg::UIntValueObject {
              UniqueID 6
              Name "recordIndex"
              Value 4294967295
            }
          }
        }
      }
    }
  }
}
)");
    }

    struct NativePathFixture
    {
        std::shared_ptr<Nif::Vector3KeyMap> path = std::make_shared<Nif::Vector3KeyMap>();
        std::shared_ptr<Nif::FloatKeyMap> percent = std::make_shared<Nif::FloatKeyMap>();
        Nif::NiPosData pathData;
        Nif::NiFloatData percentData;
        Nif::NiPathInterpolator interpolator;

        NativePathFixture()
        {
            path->mInterpolationType = Nif::InterpolationType_Quadratic;
            // x(t) = 8t^3: a straight curve whose parameter is not distance.
            path->mKeys = { { 0.f, { osg::Vec3f(), {}, {} } },
                { 1.f, { osg::Vec3f(8.f, 0.f, 0.f), osg::Vec3f(24.f, 0.f, 0.f), {} } } };
            percent->mInterpolationType = Nif::InterpolationType_Linear;
            percent->mKeys = { { 0.f, { 0.f, 0.f, 0.f } }, { 1.f, { 1.f, 0.f, 0.f } } };
            pathData.mKeyList = path;
            percentData.mKeyList = percent;
            interpolator.mRecordType = Nif::RC_NiPathInterpolator;
            interpolator.mFlags = Nif::NiPathController::Flag_ConstVelocity | Nif::NiPathController::Flag_OpenCurve;
            interpolator.mFollowAxis = 0;
            interpolator.mPathData = &pathData;
            interpolator.mPercentData = &percentData;
        }
    };

    TEST(NifOsgControllerTest, pathConstantVelocityAccountsForNonuniformStraightCurveParameterization)
    {
        NativePathFixture fixture;
        PathController controller(&fixture.interpolator);
        for (float time : { 0.f, 0.125f, 0.5f, 0.9f, 1.f })
            EXPECT_NEAR(controller.evaluate(time).mTranslation->x(), time * 8.f, 0.001f);
        PathController clone(controller, osg::CopyOp::SHALLOW_COPY);
        EXPECT_NEAR(clone.evaluate(0.3f).mTranslation->x(), 2.4f, 0.001f);
        EXPECT_NEAR(controller.evaluate(0.7f).mTranslation->x(), 5.6f, 0.001f);
        fixture.interpolator.mFlags = Nif::NiPathController::Flag_OpenCurve;
        EXPECT_FLOAT_EQ(PathController(&fixture.interpolator).evaluate(0.5f).mTranslation->x(), 1.f);

        fixture.percent->mKeys = { { 0.f, { -0.25f, 0.f, 0.f } }, { 1.f, { 1.25f, 0.f, 0.f } } };
        fixture.interpolator.mFlags |= Nif::NiPathController::Flag_ConstVelocity;
        PathController wrapped(&fixture.interpolator);
        EXPECT_NEAR(wrapped.evaluate(0.f).mTranslation->x(), 6.f, 0.001f);
        EXPECT_NEAR(wrapped.evaluate(1.f).mTranslation->x(), 2.f, 0.001f);
    }

    TEST(NifOsgControllerTest, pathFollowUsesAuthoredAxisAndFlip)
    {
        NativePathFixture fixture;
        fixture.path->mInterpolationType = Nif::InterpolationType_Linear;
        fixture.path->mKeys = { { 0.f, { osg::Vec3f(), {}, {} } },
            { 1.f, { osg::Vec3f(0.f, 8.f, 0.f), {}, {} } } };
        for (unsigned axisIndex = 0; axisIndex != 3; ++axisIndex)
        {
            fixture.interpolator.mFollowAxis = axisIndex;
            for (bool flip : { false, true })
            {
                fixture.interpolator.mFlags = Nif::NiPathController::Flag_Follow | Nif::NiPathController::Flag_OpenCurve
                    | (flip ? Nif::NiPathController::Flag_FlipFollowAxis : 0);
                PathController controller(&fixture.interpolator);
                for (float time : { 0.f, 0.5f, 1.f })
                {
                    const auto result = controller.evaluate(time);
                    ASSERT_TRUE(result.mRotation);
                    osg::Vec3f axis;
                    axis[axisIndex] = flip ? -1.f : 1.f;
                    EXPECT_LT(((*result.mRotation * axis) - osg::Vec3f(0.f, 1.f, 0.f)).length(), 1e-6f);
                    EXPECT_FALSE(result.mScale);
                }
            }
        }
    }

    TEST(NifOsgControllerTest, pathArcLengthHandlesFloatTimeRoundingAndTranslatedCoordinates)
    {
        NativePathFixture fixture;
        const float begin = 0.25f;
        const float end = std::nextafter(std::nextafter(std::nextafter(begin, 1.f), 1.f), 1.f);
        fixture.path->mInterpolationType = Nif::InterpolationType_Linear;
        fixture.path->mKeys = { { 0.f, { osg::Vec3f(256.f, -155.f, 42.f), {}, {} } },
            { begin, { osg::Vec3f(256.f, -155.f, 42.f), {}, {} } },
            { end, { osg::Vec3f(264.f, -155.f, 42.f), {}, {} } },
            { 1.f, { osg::Vec3f(264.f, -155.f, 42.f), {}, {} } } };
        // There are only four representable parameter values on this tiny
        // segment. Construction must not subdivide forever looking for a
        // nonexistent exact midpoint, or misdiagnose rounding as curvature.
        PathController controller(&fixture.interpolator);
        EXPECT_FLOAT_EQ(controller.evaluate(0.f).mTranslation->x(), 256.f);
        EXPECT_FLOAT_EQ(controller.evaluate(1.f).mTranslation->x(), 264.f);
    }

    TEST(NifOsgControllerTest, closedCurvedPathUsesDistanceAndContinuousSeamDirection)
    {
        NativePathFixture fixture;
        constexpr float tangent = 1.65685425f;
        fixture.path->mKeys = {
            { 0.f, { { 1.f, 0.f, 0.f }, { 0.f, tangent, 0.f }, { 0.f, tangent, 0.f } } },
            { 0.1f, { { 0.f, 1.f, 0.f }, { -tangent, 0.f, 0.f }, { -tangent, 0.f, 0.f } } },
            { 0.8f, { { -1.f, 0.f, 0.f }, { 0.f, -tangent, 0.f }, { 0.f, -tangent, 0.f } } },
            { 0.9f, { { 0.f, -1.f, 0.f }, { tangent, 0.f, 0.f }, { tangent, 0.f, 0.f } } },
            { 1.f, { { 1.f, 0.f, 0.f }, { 0.f, tangent, 0.f }, { 0.f, tangent, 0.f } } },
        };
        fixture.interpolator.mFlags = Nif::NiPathController::Flag_ConstVelocity | Nif::NiPathController::Flag_Follow;
        fixture.interpolator.mFollowAxis = 2;
        PathController controller(&fixture.interpolator);
        EXPECT_LT((*controller.evaluate(0.25f).mTranslation - osg::Vec3f(0.f, 1.f, 0.f)).length(), 0.001f);
        EXPECT_LT((*controller.evaluate(0.5f).mTranslation - osg::Vec3f(-1.f, 0.f, 0.f)).length(), 0.001f);
        EXPECT_LT((*controller.evaluate(0.75f).mTranslation - osg::Vec3f(0.f, -1.f, 0.f)).length(), 0.001f);
        for (float time : { 0.f, 1.f })
        {
            const auto result = controller.evaluate(time);
            ASSERT_TRUE(result.mRotation);
            EXPECT_LT(((*result.mRotation * osg::Vec3f(0.f, 0.f, 1.f)) - osg::Vec3f(0.f, 1.f, 0.f)).length(), 0.001f);
        }
    }

    TEST(NifOsgControllerTest, degeneratePathDoesNotInventRotationAndLegacyPathKeepsParameterTiming)
    {
        NativePathFixture fixture;
        Nif::NiPathController legacy;
        legacy.mPathFlags = Nif::NiPathController::Flag_ConstVelocity;
        legacy.mPathData = &fixture.pathData;
        legacy.mPercentData = &fixture.percentData;
        EXPECT_FLOAT_EQ(PathController(&legacy).evaluate(0.5f).mTranslation->x(), 1.f);
        fixture.path->mInterpolationType = Nif::InterpolationType_Linear;
        fixture.path->mKeys = { { 0.f, { osg::Vec3f(2.f, 3.f, 4.f), {}, {} } },
            { 1.f, { osg::Vec3f(2.f, 3.f, 4.f), {}, {} } } };
        fixture.interpolator.mFlags |= Nif::NiPathController::Flag_Follow;
        const auto result = PathController(&fixture.interpolator).evaluate(0.5f);
        EXPECT_EQ(result.mTranslation, osg::Vec3f(2.f, 3.f, 4.f));
        EXPECT_FALSE(result.mRotation);
    }

    TEST(NifOsgControllerTest, pathRejectsMissingMalformedAndUnsupportedData)
    {
        NativePathFixture fixture;
        fixture.interpolator.mFlags |= Nif::NiPathController::Flag_Bank;
        EXPECT_THROW(PathController{ &fixture.interpolator }, std::runtime_error);
        fixture.interpolator.mFlags = Nif::NiPathController::Flag_Follow;
        fixture.interpolator.mFollowAxis = 3;
        EXPECT_THROW(PathController{ &fixture.interpolator }, std::runtime_error);
        fixture.interpolator.mFollowAxis = 0;
        fixture.path->mInterpolationType = Nif::InterpolationType_XYZ;
        EXPECT_THROW(PathController{ &fixture.interpolator }, std::runtime_error);
        fixture.path->mInterpolationType = Nif::InterpolationType_Quadratic;
        fixture.path->mKeys.back().first = 0.f;
        EXPECT_THROW(PathController{ &fixture.interpolator }, std::runtime_error);
        fixture.path->mKeys.back().first = 1.f;
        fixture.percent->mKeys.front().second.mValue = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(PathController{ &fixture.interpolator }, std::runtime_error);
        fixture.percent->mKeys.front().second.mValue = 0.f;
        PathController controller(&fixture.interpolator);
        EXPECT_THROW(controller.evaluate(std::numeric_limits<float>::infinity()), std::runtime_error);
        fixture.path->mKeys.clear();
        EXPECT_THROW(PathController{ &fixture.interpolator }, std::runtime_error);
        fixture.interpolator.mPathData = nullptr;
        EXPECT_THROW(PathController{ &fixture.interpolator }, std::runtime_error);
    }

    TEST_F(NifOsgLoaderTest, attachesNativePathInterpolatorToItsAuthoredNode)
    {
        NativePathFixture fixture;
        Nif::NiAVObject node;
        init(node);
        Nif::NiKeyframeController key;
        init(static_cast<Nif::NiTimeController&>(key));
        key.mRecordType = Nif::RC_NiKeyframeController;
        key.mFlags = 8;
        key.mFrequency = 1.f;
        key.mTimeStop = 1.f;
        key.mData = nullptr;
        key.mInterpolator = &fixture.interpolator;
        node.mController = &key;
        Nif::NIFFile file(testNif);
        file.mRoots.push_back(&node);
        const auto result = Loader::load(file, &mImageManager, &mMaterialManager);
        class Visitor : public osg::NodeVisitor
        {
        public:
            unsigned count = 0;
            Visitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}
            void apply(osg::Node& value) override
            {
                for (auto* callback = value.getUpdateCallback(); callback; callback = callback->getNestedCallback())
                    if (dynamic_cast<PathController*>(callback))
                        ++count;
                traverse(value);
            }
        } visitor;
        result->accept(visitor);
        EXPECT_EQ(visitor.count, 1u);
    }

    TEST(NifOsgControllerTest, multiplexesOverlappingSequenceTracksOnTheSyntheticTimeline)
    {
        auto forward = std::make_shared<Nif::FloatKeyMap>();
        forward->mInterpolationType = Nif::InterpolationType_Linear;
        forward->mKeys = { { 0.f, { 0.f, 0.f, 0.f } }, { 1.f, { 10.f, 0.f, 0.f } } };
        auto backward = std::make_shared<Nif::FloatKeyMap>();
        backward->mInterpolationType = Nif::InterpolationType_Linear;
        backward->mKeys = { { 0.f, { 10.f, 0.f, 0.f } }, { 1.f, { 0.f, 0.f, 0.f } } };

        NifOsg::FloatInterpolator value({
            { 0.f, 1.f, 0.f, 1.f, forward, 0.f },
            { 2.f, 3.f, 0.f, 1.f, backward, 0.f },
        });

        EXPECT_FLOAT_EQ(value.interpKey(0.25f), 2.5f);
        EXPECT_FLOAT_EQ(value.interpKey(2.25f), 7.5f);
        EXPECT_FLOAT_EQ(value.interpKey(3.f), 0.f);
    }

    TEST(NifOsgControllerTest, ignoresUnsetBethesdaSequenceTransformComponents)
    {
        constexpr float unset = -std::numeric_limits<float>::max();
        auto rotations = std::make_shared<Nif::QuaternionKeyMap>();
        rotations->mInterpolationType = Nif::InterpolationType_Linear;
        rotations->mKeys = { { 0.f, { osg::Quat(), {}, {} } },
            { 1.f, { osg::Quat(osg::PI_2, osg::X_AXIS), {}, {} } } };

        Nif::NiKeyframeData data;
        data.mRotations = rotations;
        data.mTranslations = std::make_shared<Nif::Vector3KeyMap>();
        data.mScales = std::make_shared<Nif::FloatKeyMap>();
        Nif::NiTransformInterpolator interpolator;
        interpolator.mRecordType = Nif::RC_NiTransformInterpolator;
        interpolator.mData = Nif::NiKeyframeDataPtr(&data);
        interpolator.mDefaultValue
            = { osg::Vec3f(4.f, 5.f, 6.f), osg::Quat(unset, unset, unset, unset), unset };

        class Source final : public SceneUtil::ControllerSource
        {
        public:
            float getValue(osg::NodeVisitor*) override { return 0.5f; }
        };
        NifOsg::KeyframeController controller(
            { { 0.f, 1.f, 0.f, 1.f, &interpolator } });
        controller.setSource(std::make_shared<Source>());

        const auto transform = controller.getCurrentTransformation(nullptr);
        ASSERT_TRUE(transform.mRotation);
        EXPECT_NEAR(transform.mRotation->x(), std::sin(osg::PI_4 / 2.f), 1e-6f);
        EXPECT_EQ(transform.mTranslation, osg::Vec3f(4.f, 5.f, 6.f));
        EXPECT_FALSE(transform.mScale);
    }

    TEST(NifOsgControllerTest, evaluatesTransformBSplineSequenceTracks)
    {
        constexpr float unset = -std::numeric_limits<float>::max();
        Nif::NiBSplineData data;
        data.mFloatControlPoints = {
            0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 2.f, 0.f, 0.f, 3.f, 0.f, 0.f,
            1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f,
            1.f, 1.f, 1.f, 1.f,
        };
        Nif::NiBSplineBasisData basis;
        basis.mNumControlPoints = 4;
        Nif::NiBSplineTransformInterpolator interpolator;
        interpolator.mRecordType = Nif::RC_NiBSplineTransformInterpolator;
        interpolator.mStartTime = 10.f;
        interpolator.mStopTime = 20.f;
        interpolator.mSplineData = Nif::NiBSplineDataPtr(&data);
        interpolator.mBasisData = Nif::NiBSplineBasisDataPtr(&basis);
        interpolator.mValue = { osg::Vec3f(unset, unset, unset), osg::Quat(unset, unset, unset, unset), unset };
        interpolator.mTranslationHandle = 0;
        interpolator.mRotationHandle = 12;
        interpolator.mScaleHandle = 28;

        class Source final : public SceneUtil::ControllerSource
        {
        public:
            float getValue(osg::NodeVisitor*) override { return 1.f; }
        };
        NifOsg::KeyframeController controller(
            { { 0.f, 2.f, 10.f, 20.f, &interpolator } });
        controller.setSource(std::make_shared<Source>());

        const auto transform = controller.getCurrentTransformation(nullptr);
        ASSERT_TRUE(transform.mTranslation);
        EXPECT_NEAR(transform.mTranslation->x(), 1.5f, 1e-6f);
        EXPECT_NEAR(transform.mTranslation->y(), 0.f, 1e-6f);
        ASSERT_TRUE(transform.mRotation);
        EXPECT_NEAR(transform.mRotation->w(), 1.f, 1e-6f);
        ASSERT_TRUE(transform.mScale);
        EXPECT_NEAR(*transform.mScale, 1.f, 1e-6f);
    }

    TEST(NifOsgControllerTest, decodesCompactTransformBSplineSequenceTracks)
    {
        constexpr float unset = -std::numeric_limits<float>::max();
        Nif::NiBSplineData data;
        for (int i = 0; i < 4; ++i)
        {
            data.mCompactControlPoints.insert(
                data.mCompactControlPoints.end(), { 32767, 0, -32767 });
        }
        for (int i = 0; i < 4; ++i)
            data.mCompactControlPoints.insert(data.mCompactControlPoints.end(), { 32767, 0, 0, 0 });
        data.mCompactControlPoints.insert(data.mCompactControlPoints.end(), 4, 0);
        Nif::NiBSplineBasisData basis;
        basis.mNumControlPoints = 4;
        Nif::NiBSplineCompTransformInterpolator interpolator;
        interpolator.mRecordType = Nif::RC_NiBSplineCompTransformInterpolator;
        interpolator.mStartTime = 0.f;
        interpolator.mStopTime = 1.f;
        interpolator.mSplineData = Nif::NiBSplineDataPtr(&data);
        interpolator.mBasisData = Nif::NiBSplineBasisDataPtr(&basis);
        interpolator.mValue = { osg::Vec3f(unset, unset, unset), osg::Quat(unset, unset, unset, unset), unset };
        interpolator.mTranslationHandle = 0;
        interpolator.mRotationHandle = 12;
        interpolator.mScaleHandle = 28;
        interpolator.mTranslationOffset = 10.f;
        interpolator.mTranslationHalfRange = 2.f;
        interpolator.mRotationOffset = 0.f;
        interpolator.mRotationHalfRange = 1.f;
        interpolator.mScaleOffset = 2.f;
        interpolator.mScaleHalfRange = 1.f;

        class Source final : public SceneUtil::ControllerSource
        {
        public:
            float getValue(osg::NodeVisitor*) override { return 0.5f; }
        };
        NifOsg::KeyframeController controller(
            { { 0.f, 1.f, 0.f, 1.f, &interpolator } });
        controller.setSource(std::make_shared<Source>());

        const auto transform = controller.getCurrentTransformation(nullptr);
        ASSERT_TRUE(transform.mTranslation);
        EXPECT_NEAR(transform.mTranslation->x(), 12.f, 1e-6f);
        EXPECT_NEAR(transform.mTranslation->y(), 10.f, 1e-6f);
        EXPECT_NEAR(transform.mTranslation->z(), 8.f, 1e-6f);
        ASSERT_TRUE(transform.mRotation);
        EXPECT_NEAR(transform.mRotation->w(), 1.f, 1e-6f);
        ASSERT_TRUE(transform.mScale);
        EXPECT_NEAR(*transform.mScale, 2.f, 1e-6f);
    }

    TEST(NifOsgControllerTest, centerFacingBillboardUsesEyeToObjectDirection)
    {
        Nif::NiTransform transform = Nif::NiTransform::getIdentity();
        transform.mTranslation = osg::Vec3f(10.f, 0.f, 0.f);
        NifOsg::AutoTransform billboard(transform, NifOsg::AutoTransform::Mode::RigidFaceCenter);

        const osg::Matrixd left = billboard.computeMatrixForFrame(
            osg::Vec3d(0.f, 0.f, 0.f), osg::Vec3d(0.f, 1.f, 0.f), osg::Vec3d(0.f, 0.f, 1.f));
        const osg::Matrixd right = billboard.computeMatrixForFrame(
            osg::Vec3d(20.f, 0.f, 0.f), osg::Vec3d(0.f, 1.f, 0.f), osg::Vec3d(0.f, 0.f, 1.f));

        EXPECT_GT((left.getRotate() * osg::Vec3d(0.f, 0.f, 1.f)).x(), 0.f);
        EXPECT_LT((right.getRotate() * osg::Vec3d(0.f, 0.f, 1.f)).x(), 0.f);
        EXPECT_EQ(left.getTrans(), osg::Vec3d(10.f, 0.f, 0.f));
    }

    TEST(NifOsgParticleTest, shooterAppliesModernRadiusAndRotation)
    {
        Nif::NiPSysRotationModifier rotation;
        rotation.mRotationSpeed = 2.f;
        rotation.mRotationSpeedVariation = 0.f;
        rotation.mRotationAngle = 1.f;
        rotation.mRotationAngleVariation = 0.f;
        rotation.mRandomRotSpeedSign = false;
        rotation.mRandomAxis = false;
        rotation.mAxis.set(0.f, 0.f, 1.f);

        NifOsg::ParticleShooter shooter(0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 2.f, 0.f);
        shooter.setRadius(3.f, 0.f);
        shooter.setRotation(&rotation);
        osgParticle::Particle particle;
        shooter.shoot(&particle);

        EXPECT_FLOAT_EQ(particle.getLifeTime(), 2.f);
        EXPECT_FLOAT_EQ(particle.getRadius(), 3.f);
        EXPECT_FLOAT_EQ(particle.getSizeRange().minimum, 3.f);
        EXPECT_EQ(particle.getAngle(), osg::Vec3f(0.f, 0.f, 1.f));
        EXPECT_EQ(particle.getAngularVelocity(), osg::Vec3f(0.f, 0.f, 2.f));
    }

    TEST(NifOsgParticleTest, modernDragAttenuatesVelocityInsideItsCylinder)
    {
        Nif::NiPSysDragModifier drag;
        drag.mDragObject = nullptr;
        drag.mDragAxis.set(0.f, 0.f, 1.f);
        drag.mPercentage = .75f;
        drag.mRange = 10.f;
        drag.mRangeFalloff = 0.f;
        NifOsg::ParticleDrag affector(&drag);
        osg::ref_ptr<osgParticle::ModularProgram> program = new osgParticle::ModularProgram;
        program->setReferenceFrame(osgParticle::ParticleProcessor::RELATIVE_RF);
        affector.beginOperate(program);
        osgParticle::Particle particle;
        particle.setPosition(osg::Vec3f(1.f, 0.f, 0.f));
        particle.setVelocity(osg::Vec3f(4.f, 0.f, 0.f));

        affector.operate(&particle, 1.0);

        EXPECT_NEAR(particle.getVelocity().x(), 1.f, 1e-6f);
    }

    TEST(NifOsgParticleTest, modernSpawnCreatesConfiguredDeathGeneration)
    {
        Nif::NiPSysSpawnModifier spawn;
        spawn.mNumSpawnGenerations = 1;
        spawn.mPercentageSpawned = 1.f;
        spawn.mMinNumToSpawn = 2;
        spawn.mMaxNumToSpawn = 2;
        spawn.mSpawnSpeedVariation = 0.f;
        spawn.mSpawnDirVariation = 0.f;
        spawn.mLifespan = 5.f;
        spawn.mLifespanVariation = 0.f;
        NifOsg::ParticleSpawn affector(&spawn);
        NifOsg::ParticleSystem system;
        system.setQuota(10);
        osgParticle::Particle initial;
        initial.setLifeTime(.5f);
        initial.setVelocity(osg::Vec3f(1.f, 0.f, 0.f));
        ASSERT_NE(system.createParticle(&initial), nullptr);

        affector.operateParticles(&system, 1.0);

        EXPECT_EQ(system.numParticles(), 3);
        EXPECT_FLOAT_EQ(system.getParticle(1)->getLifeTime(), 5.f);
        EXPECT_EQ(system.getParticle(1)->getVelocity(), osg::Vec3f(1.f, 0.f, 0.f));
    }

    TEST(NifOsgParticleTest, modernPlanarColliderReflectsCrossingParticle)
    {
        Nif::NiPSysPlanarCollider collider;
        collider.mBounce = 1.f;
        collider.mColliderObject = nullptr;
        collider.mWidth = 10.f;
        collider.mHeight = 10.f;
        collider.mXAxis.set(1.f, 0.f, 0.f);
        collider.mYAxis.set(0.f, 1.f, 0.f);
        NifOsg::PlanarCollider affector(&collider);
        osg::ref_ptr<osgParticle::ModularProgram> program = new osgParticle::ModularProgram;
        program->setReferenceFrame(osgParticle::ParticleProcessor::RELATIVE_RF);
        affector.beginOperate(program);
        osgParticle::Particle particle;
        particle.setPosition(osg::Vec3f(0.f, 0.f, 1.f));
        particle.setVelocity(osg::Vec3f(0.f, 0.f, -1.f));

        affector.operate(&particle, 2.0);

        EXPECT_NEAR(particle.getVelocity().z(), 1.f, 1e-6f);
    }

    TEST(NifOsgParticleTest, modernBombAppliesSphericalImpulse)
    {
        Nif::NiPSysBombModifier bomb;
        bomb.mBombObject = nullptr;
        bomb.mBombAxis.set(0.f, 0.f, 1.f);
        bomb.mRange = 10.f;
        bomb.mStrength = 2.f;
        bomb.mDecayType = Nif::DecayType::None;
        bomb.mSymmetryType = Nif::SymmetryType::Spherical;
        NifOsg::ParticleBomb affector(&bomb);
        osg::ref_ptr<osgParticle::ModularProgram> program = new osgParticle::ModularProgram;
        program->setReferenceFrame(osgParticle::ParticleProcessor::RELATIVE_RF);
        affector.beginOperate(program);
        osgParticle::Particle particle;
        particle.setPosition(osg::Vec3f(1.f, 0.f, 0.f));

        affector.operate(&particle, 1.0);

        EXPECT_NEAR(particle.getVelocity().x(), 2.f, 1e-6f);
    }

    std::string formatOsgNodeForBSShaderProperty(std::string_view shaderPrefix)
    {
        std::ostringstream oss;
        oss << R"(
osg::Group {
  UniqueID 1
  DataVariance STATIC
  UserDataContainer TRUE {
    osg::DefaultUserDataContainer {
      UniqueID 2
      UDC_UserObjects 1 {
        osg::StringValueObject {
          UniqueID 3
          Name "fileHash"
        }
      }
    }
  }
  Children 1 {
    osg::Group {
      UniqueID 4
      DataVariance STATIC
      UserDataContainer TRUE {
        osg::DefaultUserDataContainer {
          UniqueID 5
          UDC_UserObjects 2 {
            osg::UIntValueObject {
              UniqueID 6
              Name "recordIndex"
              Value 4294967295
            }
            osg::StringValueObject {
              UniqueID 7
              Name "shaderPrefix"
              Value ")"
            << shaderPrefix << R"("
            }
          }
        }
      }
      StateSet TRUE {
        osg::StateSet {
          UniqueID 8
        }
      }
    }
  }
}
)";
        return oss.str();
    }

    std::string formatOsgNodeForBSLightingShaderProperty(std::string_view shaderPrefix)
    {
        std::ostringstream oss;
        oss << R"(
osg::Group {
  UniqueID 1
  DataVariance STATIC
  UserDataContainer TRUE {
    osg::DefaultUserDataContainer {
      UniqueID 2
      UDC_UserObjects 1 {
        osg::StringValueObject {
          UniqueID 3
          Name "fileHash"
        }
      }
    }
  }
  Children 1 {
    osg::Group {
      UniqueID 4
      DataVariance STATIC
      UserDataContainer TRUE {
        osg::DefaultUserDataContainer {
          UniqueID 5
          UDC_UserObjects 2 {
            osg::UIntValueObject {
              UniqueID 6
              Name "recordIndex"
              Value 4294967295
            }
            osg::StringValueObject {
              UniqueID 7
              Name "shaderPrefix"
              Value ")"
            << shaderPrefix << R"("
            }
          }
        }
      }
      StateSet TRUE {
        osg::StateSet {
          UniqueID 8
          ModeList 1 {
            GL_DEPTH_TEST ON
          }
          AttributeList 1 {
            osg::Depth {
              UniqueID 9
              Function LEQUAL
            }
            Value OFF
          }
        }
      }
    }
  }
}
)";
        return oss.str();
    }

    struct ShaderPrefixParams
    {
        unsigned int mShaderType;
        std::string_view mExpectedShaderPrefix;
    };

    struct NifOsgLoaderBSShaderPrefixTest : TestWithParam<ShaderPrefixParams>, BaseNifOsgLoaderTest
    {
        static constexpr std::array sParams = {
            ShaderPrefixParams{ static_cast<unsigned int>(Nif::BSShaderType::ShaderType_Default), "bs/default" },
            ShaderPrefixParams{ static_cast<unsigned int>(Nif::BSShaderType::ShaderType_NoLighting), "bs/nolighting" },
            ShaderPrefixParams{ static_cast<unsigned int>(Nif::BSShaderType::ShaderType_Tile), "bs/default" },
            ShaderPrefixParams{ std::numeric_limits<unsigned int>::max(), "bs/default" },
        };
    };

    TEST_P(NifOsgLoaderBSShaderPrefixTest, shouldAddShaderPrefix)
    {
        Nif::NiAVObject node;
        init(node);
        Nif::BSShaderPPLightingProperty property;
        property.mRecordType = Nif::RC_BSShaderPPLightingProperty;
        property.mTextureSet = nullptr;
        property.mController = nullptr;
        property.mType = GetParam().mShaderType;
        node.mProperties.push_back(Nif::RecordPtrT<Nif::NiProperty>(&property));
        Nif::NIFFile file(testNif);
        file.mRoots.push_back(&node);
        auto result = Loader::load(file, &mImageManager, &mMaterialManager);
        EXPECT_EQ(serialize(*result), formatOsgNodeForBSShaderProperty(GetParam().mExpectedShaderPrefix));
    }

    INSTANTIATE_TEST_SUITE_P(Params, NifOsgLoaderBSShaderPrefixTest, ValuesIn(NifOsgLoaderBSShaderPrefixTest::sParams));

    struct NifOsgLoaderBSLightingShaderPrefixTest : TestWithParam<ShaderPrefixParams>, BaseNifOsgLoaderTest
    {
        static constexpr std::array sParams = {
            ShaderPrefixParams{
                static_cast<unsigned int>(Nif::BSLightingShaderType::ShaderType_Default), "bs/default" },
            ShaderPrefixParams{ static_cast<unsigned int>(Nif::BSLightingShaderType::ShaderType_Cloud), "bs/default" },
            ShaderPrefixParams{ std::numeric_limits<unsigned int>::max(), "bs/default" },
        };
    };

    TEST_P(NifOsgLoaderBSLightingShaderPrefixTest, shouldAddShaderPrefix)
    {
        Nif::NiAVObject node;
        init(node);
        Nif::BSLightingShaderProperty property;
        property.mRecordType = Nif::RC_BSLightingShaderProperty;
        property.mTextureSet = nullptr;
        property.mController = nullptr;
        property.mType = GetParam().mShaderType;
        property.mShaderFlags1 |= Nif::BSShaderFlags1::BSSFlag1_DepthTest;
        property.mShaderFlags2 |= Nif::BSShaderFlags2::BSSFlag2_DepthWrite;
        node.mProperties.push_back(Nif::RecordPtrT<Nif::NiProperty>(&property));
        Nif::NIFFile file(testNif);
        file.mRoots.push_back(&node);
        auto result = Loader::load(file, &mImageManager, &mMaterialManager);
        EXPECT_EQ(serialize(*result), formatOsgNodeForBSLightingShaderProperty(GetParam().mExpectedShaderPrefix));
    }

    INSTANTIATE_TEST_SUITE_P(
        Params, NifOsgLoaderBSLightingShaderPrefixTest, ValuesIn(NifOsgLoaderBSLightingShaderPrefixTest::sParams));
}

namespace
{
    TEST(ESM4NativeAnimationMetadata, OriginalSequenceCoordinatesAndCopies)
    {
        Nif::NIFFile file(VFS::Path::Normalized("handtohandattackleft.kf"));
        file.mVersion = Nif::NIFFile::VER_OB;
        auto keys = std::make_unique<Nif::NiTextKeyExtraData>();
        keys->mRecordType = Nif::RC_NiTextKeyExtraData;
        keys->mList = {{10.f, "Start"}, {10.25f, "HiT"}, {11.f, "a:R"},
            {12.f, "End"}, {10.5f, " Hit\r\nSound: RawName "}};
        auto sequence = std::make_unique<Nif::NiControllerSequence>();
        sequence->mRecordType = Nif::RC_NiControllerSequence;
        sequence->mName = "InternalLabel";
        sequence->mStartTime = 10;
        sequence->mStopTime = 12;
        sequence->mFrequency = 1.25f;
        sequence->mTextKeys = keys.get();
        file.mRecords.push_back(std::move(sequence));
        file.mRecords.push_back(std::move(keys));
        osg::ref_ptr<SceneUtil::KeyframeHolder> holder = new SceneUtil::KeyframeHolder;
        NifOsg::Loader::loadKf(file, *holder);
        ASSERT_EQ(holder->mControllerSequences.size(), 1);
        const auto& original = holder->mControllerSequences.front();
        EXPECT_EQ(original.mGroup, "handtohandattackleft");
        EXPECT_EQ(original.mStartTime, 10);
        EXPECT_EQ(original.mStopTime, 12);
        EXPECT_EQ(original.mFrequency, 1.25f);
        const std::vector<std::pair<float, std::string>> expected{{10.f, "Start"}, {10.25f, "HiT"},
            {11.f, "a:R"}, {12.f, "End"}, {10.5f, " Hit\r\nSound: RawName "}};
        EXPECT_EQ(original.mTextKeys, expected); // Original order, text and absolute times.
        EXPECT_EQ(holder->mTextKeys.findGroupStart("handtohandattackleft")->first, 0);
        bool normalizedHit = false;
        for (const auto& [time, text] : holder->mTextKeys)
            if (text == "hit" && time == .25f) normalizedHit = true;
        EXPECT_TRUE(normalizedHit); // Shared renderer behavior retains its coordinates.
        osg::ref_ptr<SceneUtil::KeyframeHolder> copied
            = new SceneUtil::KeyframeHolder(*holder, osg::CopyOp::SHALLOW_COPY);
        EXPECT_EQ(copied->mControllerSequences, holder->mControllerSequences);
        copied->mControllerSequences[0].mTextKeys[0].second = "changed";
        EXPECT_EQ(holder->mControllerSequences[0].mTextKeys[0].second, "Start");
        Nif::NIFFile noSequence(VFS::Path::Normalized("empty.kf"));
        noSequence.mVersion = Nif::NIFFile::VER_MW;
        osg::ref_ptr<SceneUtil::KeyframeHolder> empty = new SceneUtil::KeyframeHolder;
        NifOsg::Loader::loadKf(noSequence, *empty);
        EXPECT_TRUE(empty->mControllerSequences.empty());
    }
}
