#include "../nif/node.hpp"

#include <components/nif/node.hpp>
#include <components/nif/extra.hpp>
#include <components/sceneutil/keyframe.hpp>
#include <components/nif/property.hpp>
#include <components/nif/physics.hpp>
#include <components/nifosg/matrixtransform.hpp>
#include <components/nifosg/nifloader.hpp>
#include <components/nifosg/autotransform.hpp>
#include <components/nifosg/controller.hpp>
#include <components/nifosg/particle.hpp>
#include <components/resource/bgsmfilemanager.hpp>
#include <components/resource/imagemanager.hpp>
#include <components/sceneutil/serialize.hpp>
#include <components/sceneutil/optimizer.hpp>
#include <components/sceneutil/clone.hpp>
#include <components/sceneutil/nifscenetraversal.hpp>
#include <osg/FrameStamp>
#include <osg/Geometry>
#include <osg/Geode>
#include <osg/ShapeDrawable>
#include <osg/Transform>
#include <algorithm>
#include <components/vfs/manager.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <osgDB/Registry>
#include <osgDB/SharedStateManager>

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

    TEST_F(NifOsgLoaderTest, oblivionScabbardRemainsSeparateFromBladeAfterOptimization)
    {
        Nif::NiNode root;
        init(root);
        root.mRecordType = Nif::RC_NiNode;
        root.mName = "ShortSword";
        Nif::NiTriShape scabbard, blade;
        init(scabbard);
        init(blade);
        scabbard.mShaderProperty = nullptr;
        scabbard.mAlphaProperty = nullptr;
        blade.mShaderProperty = nullptr;
        blade.mAlphaProperty = nullptr;
        scabbard.mName = "Scb:0";
        blade.mName = "ShortSword:0";
        Nif::NiTriShapeData geometry{};
        geometry.mRecordType = Nif::RC_NiTriShapeData;
        geometry.mNumVertices = 3;
        geometry.mNumTriangles = 1;
        geometry.mVertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
        geometry.mTriangles = { 0, 1, 2 };
        scabbard.mData = &geometry;
        blade.mData = &geometry;
        root.mChildren = { Nif::NiAVObjectPtr(&scabbard), Nif::NiAVObjectPtr(&blade) };
        Nif::NIFFile file(testNif);
        file.mVersion = 0x14000004; // Original stock shortsword version.
        file.mRoots.push_back(&root);
        auto loaded = Loader::load(file, &mImageManager, &mMaterialManager);
        // The scene manager permits flattening ordinary NIF property nodes;
        // the optimizer's default callback otherwise keeps their StateSets.
        struct ModelPermissions : SceneUtil::Optimizer::IsOperationPermissibleForObjectCallback
        {
            bool isOperationPermissibleForObjectImplementation(const SceneUtil::Optimizer* optimizer,
                const osg::Node* node, unsigned int option) const override
            {
                return node->getDataVariance() != osg::Object::DYNAMIC
                    && (option & optimizer->getPermissibleOptimizationsForObject(node)) != 0;
            }
        };
        SceneUtil::Optimizer optimizer;
        optimizer.setIsOperationPermissibleForObjectCallback(new ModelPermissions);
        osg::ref_ptr<osgDB::SharedStateManager> shared = new osgDB::SharedStateManager;
        optimizer.setSharedStateManager(shared, nullptr);
        optimizer.optimize(loaded, SceneUtil::Optimizer::FLATTEN_STATIC_TRANSFORMS
                | SceneUtil::Optimizer::REMOVE_REDUNDANT_NODES | SceneUtil::Optimizer::MERGE_GEOMETRY
                | SceneUtil::Optimizer::SHARE_DUPLICATE_STATE);
        struct GeometryVisitor : osg::NodeVisitor
        {
            std::vector<osg::Geometry*> meshes;
            GeometryVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}
            void apply(osg::Geometry& mesh) override { meshes.push_back(&mesh); }
        } before;
        loaded->accept(before);
        ASSERT_EQ(before.meshes.size(), 2);
        auto sheath = std::ranges::find_if(before.meshes,
            [](const auto* mesh) { return mesh->getName() == "Scb:0"; });
        ASSERT_NE(sheath, before.meshes.end());
        (*sheath)->setNodeMask(0);
        GeometryVisitor after;
        loaded->accept(after);
        ASSERT_EQ(after.meshes.size(), 1);
        EXPECT_EQ(after.meshes.front()->getName(), "ShortSword:0");
        ASSERT_NE(after.meshes.front()->getVertexArray(), nullptr);
        EXPECT_EQ(after.meshes.front()->getVertexArray()->getNumElements(), 3);
    }

    TEST_F(NifOsgLoaderTest, OblivionCollisionTargetsRetainMutableRecordTransformsAfterOptimization)
    {
        for (const auto version : std::array<unsigned, 2>{0x14000004u, Nif::NIFFile::VER_OB})
        for (const bool childTarget : {false, true})
        {
            SCOPED_TRACE(version);
            SCOPED_TRACE(childTarget);
            Nif::NiNode root{}, child{};
            init(root); init(child);
            root.mRecordType = child.mRecordType = Nif::RC_NiNode;
            root.mRecordIndex = 0; child.mRecordIndex = 1;
            root.mName = "WeaponRoot"; child.mName = "PhysicalChild";
            Nif::bhkCollisionObject collision{};
            collision.mRecordType = Nif::RC_bhkCollisionObject;
            auto& target = childTarget ? child : root;
            collision.mTarget = Nif::NiAVObjectPtr(&target);
            target.mCollision = Nif::NiCollisionObjectPtr(&collision);
            if (childTarget)
            {
                child.mParents.push_back(&root);
                child.mTransform.mTranslation = {3, 4, 5};
                root.mChildren = {Nif::NiAVObjectPtr(&child)};
            }
            Nif::NIFFile file(testNif);
            file.mVersion = version; file.mRoots.push_back(&root);
            auto loaded = Loader::load(file, &mImageManager, &mMaterialManager);
            struct Permissions : SceneUtil::Optimizer::IsOperationPermissibleForObjectCallback
            {
                bool isOperationPermissibleForObjectImplementation(const SceneUtil::Optimizer* optimizer,
                    const osg::Node* node, unsigned option) const override
                {
                    return node->getDataVariance() != osg::Object::DYNAMIC
                        && (option & optimizer->getPermissibleOptimizationsForObject(node)) != 0;
                }
            };
            SceneUtil::Optimizer optimizer;
            optimizer.setIsOperationPermissibleForObjectCallback(new Permissions);
            osg::ref_ptr<osgDB::SharedStateManager> shared = new osgDB::SharedStateManager;
            optimizer.setSharedStateManager(shared, nullptr);
            optimizer.optimize(loaded, SceneUtil::Optimizer::FLATTEN_STATIC_TRANSFORMS
                | SceneUtil::Optimizer::REMOVE_REDUNDANT_NODES);
            FindGroupByRecordIndex find(target.mRecordIndex);
            loaded->accept(find);
            auto* transform = dynamic_cast<NifOsg::MatrixTransform*>(find.mFound);
            ASSERT_NE(transform, nullptr);
            EXPECT_EQ(transform->getDataVariance(), osg::Object::DYNAMIC);
            EXPECT_EQ(transform->getName(), target.mName);
            EXPECT_EQ(transform->getMatrix().getTrans(), osg::Vec3d(target.mTransform.mTranslation));
            EXPECT_EQ(transform->mScale, 1.f);
            // A physical child also needs a private mutable asset root: a new
            // placed REFR replaces that root's authored transform.
            FindGroupByRecordIndex findRoot(0);
            loaded->accept(findRoot);
            ASSERT_NE(dynamic_cast<NifOsg::MatrixTransform*>(findRoot.mFound), nullptr);
            EXPECT_EQ(findRoot.mFound->getDataVariance(), osg::Object::DYNAMIC);
        }
    }

    TEST_F(NifOsgLoaderTest, MorrowindIdentityRootKeepsStaticGroupOptimization)
    {
        Nif::NiNode root{};
        init(root); root.mRecordType = Nif::RC_NiNode; root.mRecordIndex = 0;
        Nif::bhkCollisionObject collision{};
        root.mCollision = Nif::NiCollisionObjectPtr(&collision);
        Nif::NIFFile file(testNif); file.mVersion = Nif::NIFFile::VER_MW; file.mRoots.push_back(&root);
        auto loaded = Loader::load(file, &mImageManager, &mMaterialManager);
        FindGroupByRecordIndex find(0); loaded->accept(find);
        ASSERT_NE(find.mFound, nullptr);
        EXPECT_EQ(dynamic_cast<osg::Transform*>(find.mFound), nullptr);
        EXPECT_EQ(find.mFound->getDataVariance(), osg::Object::STATIC);
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

    struct ESM4NativeBlendLoaderTest : NifOsgLoaderTest {};

    TEST_F(ESM4NativeBlendLoaderTest, RetainsInactiveControllerSettingsAndAmbiguity)
    {
        for (bool duplicate : {false, true})
        {
            Nif::NiNode node{};
            init(node);
            node.mRecordType = Nif::RC_NiNode;
            Nif::NiBlendTransformInterpolator blend{};
            blend.mRecordType = Nif::RC_NiBlendTransformInterpolator;
            blend.mWeightThreshold = .125f;
            blend.mFlags = Nif::NiBlendInterpolator::Flag_OnlyUseHighestWeight;
            Nif::NiKeyframeController first{}, second{};
            for (auto* controller : {&first, &second})
            {
                init(static_cast<Nif::NiTimeController&>(*controller));
                controller->mRecordType = Nif::RC_NiKeyframeController;
                controller->mInterpolator = &blend;
            }
            if (duplicate) first.mNext = &second;
            node.mController = &first;
            Nif::NIFFile file(testNif);
            file.mVersion = Nif::NIFFile::VER_OB;
            file.mRoots.push_back(&node);
            const auto result = Loader::load(file, &mImageManager, &mMaterialManager);
            struct Inspect : osg::NodeVisitor
            {
                unsigned mFound = 0;
                bool mAmbiguous = false;
                Inspect() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}
                void apply(osg::Node& value) override
                {
                    float threshold = 0;
                    if (value.getUserValue("nativeSequenceBlendThreshold", threshold))
                    {
                        ++mFound;
                        EXPECT_EQ(threshold, .125f);
                        bool highest = false;
                        EXPECT_TRUE(value.getUserValue("nativeSequenceBlendOnlyHighest", highest));
                        EXPECT_TRUE(highest);
                        value.getUserValue("nativeSequenceBlendAmbiguous", mAmbiguous);
                    }
                    traverse(value);
                }
            } inspect;
            result->accept(inspect);
            EXPECT_EQ(inspect.mFound, 1u);
            EXPECT_EQ(inspect.mAmbiguous, duplicate);
        }
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
        sequence->mName = "AttackLeft_Hook";
        sequence->mAccumRootName = " AuthoredRoot ";
        sequence->mStartTime = 10;
        sequence->mStopTime = 12;
        sequence->mFrequency = 1.25f;
        sequence->mWeight = .625f;
        sequence->mExtrapolationMode = static_cast<Nif::NiTimeController::ExtrapolationMode>(1);
        sequence->mTextKeys = keys.get();
        auto transform = std::make_unique<Nif::NiTransformInterpolator>();
        transform->mRecordType = Nif::RC_NiTransformInterpolator;
        transform->mDefaultValue = Nif::NiQuatTransform::getIdentity();
        transform->mData = nullptr;
        auto unsupported = std::make_unique<Nif::NiFloatInterpolator>();
        unsupported->mRecordType = Nif::RC_NiFloatInterpolator;
        unsupported->mDefaultValue = 0;
        unsupported->mData = nullptr;
        Nif::ControlledBlock fallback{};
        fallback.mTargetName = "FallbackBone";
        fallback.mInterpolator = transform.get();
        fallback.mController = nullptr;
        fallback.mBlendInterpolator = nullptr;
        fallback.mStringPalette = nullptr;
        fallback.mPriority = 128;
        Nif::ControlledBlock named = fallback;
        named.mNodeName = " Bone ";
        named.mPriority = 255;
        Nif::ControlledBlock duplicate = named;
        duplicate.mPriority = 1;
        Nif::ControlledBlock ignored = named;
        ignored.mInterpolator = unsupported.get();
        sequence->mControlledBlocks = {fallback, named, duplicate, ignored};
        file.mRecords.push_back(std::move(sequence));
        file.mRecords.push_back(std::move(keys));
        file.mRecords.push_back(std::move(transform));
        file.mRecords.push_back(std::move(unsupported));
        osg::ref_ptr<SceneUtil::KeyframeHolder> holder = new SceneUtil::KeyframeHolder;
        NifOsg::Loader::loadKf(file, *holder);
        ASSERT_EQ(holder->mControllerSequences.size(), 1);
        const auto& original = holder->mControllerSequences.front();
        EXPECT_EQ(original.mGroup, "handtohandattackleft");
        EXPECT_EQ(original.mSequenceName, "AttackLeft_Hook");
        EXPECT_EQ(original.mAccumRootName, " AuthoredRoot ");
        EXPECT_EQ(original.mStartTime, 10);
        EXPECT_EQ(original.mStopTime, 12);
        EXPECT_EQ(original.mFrequency, 1.25f);
        EXPECT_EQ(original.mWeight, .625f);
        const std::vector<std::pair<std::string, std::uint8_t>> priorities{
            {"FallbackBone", 128}, {" Bone ", 255}, {" Bone ", 1}};
        EXPECT_EQ(original.mTransformPriorities, priorities);
        EXPECT_EQ(original.mCycleType, 1u);
        EXPECT_EQ(original.mTimelineStart, 0);
        EXPECT_EQ(original.mTimelineStop, 2);
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
        copied->mControllerSequences[0].mSequenceName = "Recoil_01";
        EXPECT_EQ(original.mSequenceName, "AttackLeft_Hook");
        copied->mControllerSequences[0].mAccumRootName = "OtherRoot";
        EXPECT_EQ(holder->mControllerSequences[0].mAccumRootName, " AuthoredRoot ");
        copied->mControllerSequences[0].mTextKeys[0].second = "changed";
        EXPECT_EQ(holder->mControllerSequences[0].mTextKeys[0].second, "Start");
        copied->mControllerSequences[0].mWeight = 2;
        copied->mControllerSequences[0].mTransformPriorities[0].second = 0;
        EXPECT_EQ(original.mWeight, .625f);
        EXPECT_EQ(original.mTransformPriorities, priorities);
        Nif::NIFFile noSequence(VFS::Path::Normalized("empty.kf"));
        noSequence.mVersion = Nif::NIFFile::VER_MW;
        osg::ref_ptr<SceneUtil::KeyframeHolder> empty = new SceneUtil::KeyframeHolder;
        NifOsg::Loader::loadKf(noSequence, *empty);
        EXPECT_TRUE(empty->mControllerSequences.empty());
    }
}

namespace
{
    void expectTraversalMatrixNear(const osg::Matrixd& actual, const osg::Matrixd& expected)
    {
        for (unsigned i = 0; i < 16; ++i) EXPECT_NEAR(actual.ptr()[i], expected.ptr()[i], 1e-12) << i;
    }

    TEST(NifTraversalMatrixTest, DefaultAndClearRetainOriginalRelativeAndAbsoluteTransformBehavior)
    {
        osg::ref_ptr<NifOsg::MatrixTransform> node = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        node->setRotation(osg::Quat(.3, osg::Vec3d(0, 1, 0)));
        node->setTranslation({3, 4, 5}); node->setScale(2);
        osg::ref_ptr<osg::MatrixTransform> original = new osg::MatrixTransform(node->getMatrix());
        for (auto frame : {osg::Transform::RELATIVE_RF, osg::Transform::ABSOLUTE_RF,
                 osg::Transform::ABSOLUTE_RF_INHERIT_VIEWPOINT})
        {
            node->setReferenceFrame(frame); original->setReferenceFrame(frame);
            for (bool clear : {false, true})
            {
                if (clear)
                {
                    node->setTraversalMatrix(osg::Matrixd::translate(40, 50, 60));
                    node->clearTraversalMatrix();
                }
                ASSERT_EQ(node->getTraversalMatrix().getMatrix(), nullptr);
                auto actual = osg::Matrixd::translate(100, 200, 300), expected = actual;
                node->computeLocalToWorldMatrix(actual, nullptr); original->computeLocalToWorldMatrix(expected, nullptr);
                expectTraversalMatrixNear(actual, expected);
                actual.makeTranslate(100, 200, 300); expected = actual;
                node->computeWorldToLocalMatrix(actual, nullptr); original->computeWorldToLocalMatrix(expected, nullptr);
                expectTraversalMatrixNear(actual, expected);
            }
        }
    }

    TEST(NifTraversalMatrixTest, WorldInverseAndBoundsUseProjectionWhileLocalMatrixAndNifCachesStayAuthored)
    {
        osg::ref_ptr<osg::MatrixTransform> outer = new osg::MatrixTransform;
        outer->setMatrix(osg::Matrixd::rotate(.4, osg::Vec3d(0, 0, 1)) * osg::Matrixd::translate(100, 20, 50));
        osg::ref_ptr<NifOsg::MatrixTransform> root = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        osg::ref_ptr<NifOsg::MatrixTransform> child = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        root->setTranslation({1000, 2000, 3000}); root->setTraversalMatrix(osg::Matrixd::identity());
        child->setTranslation({3, 4, 5}); child->setScale(2);
        const auto author = child->getMatrix(); const auto rotation = child->mRotationScale;
        const auto desired = osg::Matrixd::rotate(.7, osg::Vec3d(0, 1, 0)) * osg::Matrixd::translate(120, 30, 40);
        const auto local = desired * osg::Matrixd::inverse(outer->getMatrix());
        child->setTraversalMatrix(local);
        outer->addChild(root); root->addChild(child);
        osg::ref_ptr<osg::Geode> geometry = new osg::Geode;
        geometry->addDrawable(new osg::ShapeDrawable(new osg::Sphere({}, 1)));
        child->addChild(geometry);
        const auto worlds = child->getWorldMatrices(); ASSERT_EQ(worlds.size(), 1u);
        expectTraversalMatrixNear(worlds[0], desired);
        const osg::NodePath path{outer.get(), root.get(), child.get()};
        expectTraversalMatrixNear(osg::computeWorldToLocal(path), osg::Matrixd::inverse(desired));
        EXPECT_EQ(child->getMatrix(), author); EXPECT_EQ(child->mScale, 2.f);
        for (unsigned r = 0; r < 3; ++r) for (unsigned c = 0; c < 3; ++c)
            EXPECT_EQ(child->mRotationScale.mValues[r][c], rotation.mValues[r][c]);
        EXPECT_NEAR((osg::Vec3d(child->getBound().center()) - local.getTrans()).length(), 0, 1e-5);
        child->setTranslation({-8, 9, 10}); child->setScale(3);
        EXPECT_EQ(child->getMatrix().getTrans(), osg::Vec3d(-8, 9, 10));
        expectTraversalMatrixNear(child->getWorldMatrices()[0], desired);
        child->clearTraversalMatrix();
        expectTraversalMatrixNear(child->getWorldMatrices()[0], child->getMatrix() * outer->getMatrix());
    }

    TEST(NifTraversalMatrixTest, PreparedImmutableHandlesCloneAndRestoreIndependently)
    {
        osg::ref_ptr<NifOsg::MatrixTransform> node = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        const NifOsg::MatrixTransform::TraversalMatrix saved(osg::Matrixd::translate(40, 50, 60));
        node->setTraversalMatrix(saved);
        osg::ref_ptr<NifOsg::MatrixTransform> copy
            = new NifOsg::MatrixTransform(*node, osg::CopyOp::DEEP_COPY_ALL);
        node->setTraversalMatrix(osg::Matrixd::translate(70, 80, 90));
        ASSERT_NE(copy->getTraversalMatrix().getMatrix(), nullptr);
        EXPECT_EQ(copy->getTraversalMatrix().getMatrix()->getTrans(), osg::Vec3d(40, 50, 60));
        copy->clearTraversalMatrix();
        node->setTraversalMatrix(saved);
        EXPECT_EQ(node->getTraversalMatrix(), saved);
        EXPECT_EQ(copy->getTraversalMatrix().getMatrix(), nullptr);
        node->setTraversalMatrix(osg::Matrixd::translate(40, 50, 60));
        EXPECT_EQ(node->getTraversalMatrix(), saved);
        auto signedZero = *saved.getMatrix(); signedZero(0, 1) = -0.;
        node->setTraversalMatrix(signedZero);
        EXPECT_NE(node->getTraversalMatrix(), saved);
    }

    TEST(NifTraversalMatrixTest, InvalidProjectionRejectsBeforeChangingEitherPose)
    {
        osg::ref_ptr<NifOsg::MatrixTransform> node = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        node->setTranslation({3, 4, 5});
        node->setTraversalMatrix(osg::Matrixd::translate(10, 20, 30));
        const auto author = node->getMatrix(); const auto saved = node->getTraversalMatrix();
        auto nonfinite = osg::Matrixd::identity(); nonfinite(0, 0) = std::numeric_limits<double>::quiet_NaN();
        auto infinite = osg::Matrixd::identity(); infinite(3, 0) = std::numeric_limits<double>::infinity();
        for (const auto& invalid : {nonfinite, infinite, osg::Matrixd::scale(0, 1, 1),
                 osg::Matrixd::scale(std::numeric_limits<double>::denorm_min(), 1, 1)})
        {
            EXPECT_THROW(node->setTraversalMatrix(invalid), std::invalid_argument);
            EXPECT_EQ(node->getMatrix(), author); EXPECT_EQ(node->getTraversalMatrix(), saved);
        }
    }

    TEST(NifTraversalMatrixTest, ProjectionUsesAbsoluteReferenceFramesWithoutMutatingAuthorInverse)
    {
        osg::ref_ptr<NifOsg::MatrixTransform> node = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        node->setTranslation({3, 4, 5});
        const auto projection = osg::Matrixd::rotate(.7, osg::Vec3d(0, 0, 1)) * osg::Matrixd::translate(10, 20, 30);
        node->setTraversalMatrix(projection);
        for (auto frame : {osg::Transform::ABSOLUTE_RF, osg::Transform::ABSOLUTE_RF_INHERIT_VIEWPOINT})
        {
            node->setReferenceFrame(frame);
            auto world = osg::Matrixd::translate(100, 200, 300), inverse = world;
            node->computeLocalToWorldMatrix(world, nullptr); node->computeWorldToLocalMatrix(inverse, nullptr);
            expectTraversalMatrixNear(world, projection); expectTraversalMatrixNear(inverse, osg::Matrixd::inverse(projection));
        }
        EXPECT_EQ(node->getInverseMatrix().getTrans(), osg::Vec3d(-3, -4, -5));
    }

    TEST(NifTraversalMatrixTest, KeyframeMissingChannelsReadAuthoredComponentsWhileTraversalStaysProjected)
    {
        constexpr float unset = -std::numeric_limits<float>::max();
        Nif::NiTransformInterpolator interpolator;
        interpolator.mRecordType = Nif::RC_NiTransformInterpolator; interpolator.mData = nullptr;
        interpolator.mDefaultValue = {osg::Vec3f(4, 5, 6), osg::Quat(unset, unset, unset, unset), unset};
        class Source : public SceneUtil::ControllerSource
        {
            float getValue(osg::NodeVisitor*) override { return .5f; }
        };
        NifOsg::KeyframeController controller({{0.f, 1.f, 0.f, 1.f, &interpolator}});
        controller.setSource(std::make_shared<Source>());
        osg::ref_ptr<NifOsg::MatrixTransform> node = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        node->setRotation(osg::Quat(.3, osg::Vec3d(0, 0, 1))); node->setScale(3);
        const auto rotation = node->mRotationScale;
        const auto projection = osg::Matrixd::translate(100, 200, 300);
        node->setTraversalMatrix(projection);
        osg::NodeVisitor visitor(osg::NodeVisitor::UPDATE_VISITOR);
        controller(node.get(), &visitor);
        EXPECT_EQ(node->getMatrix().getTrans(), osg::Vec3d(4, 5, 6)); EXPECT_EQ(node->mScale, 3.f);
        for (unsigned r = 0; r < 3; ++r) for (unsigned c = 0; c < 3; ++c)
            EXPECT_EQ(node->mRotationScale.mValues[r][c], rotation.mValues[r][c]);
        expectTraversalMatrixNear(node->getWorldMatrices()[0], projection);
    }

    TEST(NifTraversalMatrixTest, BillboardHonorsPreparedTraversalAndClearsBackToItsCameraMode)
    {
        osg::ref_ptr<NifOsg::AutoTransform> node = new NifOsg::AutoTransform(Nif::NiTransform::getIdentity());
        node->setTranslation({3, 4, 5});
        osg::Matrixd before; node->computeLocalToWorldMatrix(before, nullptr);
        const auto projection = osg::Matrixd::rotate(.4, osg::Vec3d(0, 1, 0)) * osg::Matrixd::translate(100, 200, 300);
        node->setTraversalMatrix(projection);
        osg::Matrixd world, inverse;
        node->computeLocalToWorldMatrix(world, nullptr); node->computeWorldToLocalMatrix(inverse, nullptr);
        expectTraversalMatrixNear(world, projection); expectTraversalMatrixNear(inverse, osg::Matrixd::inverse(projection));
        node->clearTraversalMatrix(); world.makeIdentity(); node->computeLocalToWorldMatrix(world, nullptr);
        expectTraversalMatrixNear(world, before);
    }
}

namespace
{
    TEST_F(NifOsgLoaderTest, OblivionPhysicalHierarchyRetainsPrivateHelperCachesAndSourcePolicyInputs)
    {
        for (const auto version : std::array<unsigned, 2>{0x14000004u, Nif::NIFFile::VER_OB})
        {
            SCOPED_TRACE(version);
            Nif::NiNode root{}, helper{}, physical{}, sibling{};
            std::array<Nif::NiNode*, 4> records{&root, &helper, &physical, &sibling};
            for (unsigned i = 0; i < records.size(); ++i)
            {
                init(*records[i]); records[i]->mRecordType = Nif::RC_NiNode;
                records[i]->mRecordIndex = i; records[i]->mFlags = 0x1234000eu + i * 16;
                records[i]->mName = "node" + std::to_string(i);
            }
            helper.mParents.push_back(&root); physical.mParents.push_back(&helper); sibling.mParents.push_back(&root);
            root.mChildren = {Nif::NiAVObjectPtr(&helper), Nif::NiAVObjectPtr(&sibling)};
            helper.mChildren = {Nif::NiAVObjectPtr(&physical)};
            helper.mTransform.mTranslation.x() = 3; helper.mTransform.mScale = .75f;
            physical.mTransform.mTranslation.x() = 5;
            Nif::bhkCollisionObject collision{}; collision.mRecordType = Nif::RC_bhkCollisionObject;
            collision.mTarget = Nif::NiAVObjectPtr(&physical); physical.mCollision = Nif::NiCollisionObjectPtr(&collision);
            Nif::NIFFile file(testNif); file.mVersion = version; file.mRoots.push_back(&root);
            auto loaded = Loader::load(file, &mImageManager, &mMaterialManager);
            struct Permissions : SceneUtil::Optimizer::IsOperationPermissibleForObjectCallback
            {
                bool isOperationPermissibleForObjectImplementation(const SceneUtil::Optimizer* optimizer,
                    const osg::Node* node, unsigned option) const override
                {
                    return node->getDataVariance() != osg::Object::DYNAMIC
                        && (option & optimizer->getPermissibleOptimizationsForObject(node)) != 0;
                }
            };
            SceneUtil::Optimizer optimizer;
            optimizer.setIsOperationPermissibleForObjectCallback(new Permissions);
            optimizer.optimize(loaded, SceneUtil::Optimizer::FLATTEN_STATIC_TRANSFORMS
                | SceneUtil::Optimizer::REMOVE_REDUNDANT_NODES);
            SceneUtil::CopyOp copyA, copyB;
            osg::ref_ptr<osg::Node> first = osg::clone(loaded.get(), copyA), second = osg::clone(loaded.get(), copyB);
            copyA.remapControllerTargets(); copyB.remapControllerTargets();
            const auto find = [](osg::Node& tree, unsigned record) {
                FindGroupByRecordIndex visitor(record); tree.accept(visitor);
                return dynamic_cast<NifOsg::MatrixTransform*>(visitor.mFound);
            };
            for (unsigned i = 0; i < records.size(); ++i)
            {
                auto* source = find(*loaded, i); auto* a = find(*first, i); auto* b = find(*second, i);
                ASSERT_NE(source, nullptr); ASSERT_NE(a, nullptr); ASSERT_NE(b, nullptr);
                EXPECT_NE(source, a); EXPECT_NE(source, b); EXPECT_NE(a, b);
                EXPECT_EQ(source->getDataVariance(), osg::Object::DYNAMIC);
                EXPECT_EQ(source->getControllerTransform().mTranslation, records[i]->mTransform.mTranslation);
                EXPECT_EQ(source->mScale, records[i]->mTransform.mScale);
                unsigned type = ~0u, flags = 0;
                ASSERT_TRUE(a->getUserValue("nativeNifRecordType", type));
                ASSERT_TRUE(a->getUserValue("nativeNifFlags", flags));
                EXPECT_EQ(type, static_cast<unsigned>(Nif::RC_NiNode)); EXPECT_EQ(flags, records[i]->mFlags);
                // Source metadata is read-only. OSG may share ValueObjects
                // between copied containers; live flags belong to the owner.
                ASSERT_TRUE(b->getUserValue("nativeNifFlags", flags)); EXPECT_EQ(flags, records[i]->mFlags);
                ASSERT_TRUE(source->getUserValue("nativeNifFlags", flags)); EXPECT_EQ(flags, records[i]->mFlags);
                // Literal original LoadBinary tail outputs; higher authored
                // padding is not part of the native runtime WORD field.
                const std::array<unsigned, 4> loadedFlags{14, 14, 46, 46};
                for (auto* instance : {source, a, b})
                {
                    ASSERT_TRUE(instance->getUserValue("nativeNifLoadedFlags", flags));
                    EXPECT_EQ(flags, loadedFlags[i]);
                }
            }
            auto* firstRoot = find(*first, 0); auto* firstTarget = find(*first, 2);
            firstRoot->setTranslation({100, 0, 0});
            SceneUtil::NifSceneTraversal traversal(*firstRoot);
            EXPECT_FLOAT_EQ(traversal.cachedWorld(*firstTarget).mTranslation.x(), 106.75f);
            firstRoot->setTranslation({200, 0, 0});
            traversal.update(osg::FrameStamp{}, 1);
            EXPECT_FLOAT_EQ(traversal.cachedWorld(*firstTarget).mTranslation.x(), 206.75f);
            EXPECT_NEAR(firstTarget->getWorldMatrices().front().getTrans().x(), 206.75, 1e-10);
            EXPECT_EQ(find(*loaded, 0)->getTraversalMatrix().getMatrix(), nullptr);
            EXPECT_EQ(find(*second, 2)->getTraversalMatrix().getMatrix(), nullptr);
            EXPECT_EQ(find(*loaded, 2)->getWorldMatrices().front().getTrans().x(), 6.75);
            EXPECT_EQ(find(*second, 2)->getWorldMatrices().front().getTrans().x(), 6.75);
        }
    }

    TEST_F(NifOsgLoaderTest, NonphysicalOblivionRootKeepsOptimizationWithoutNativePhysicsMetadata)
    {
        Nif::NiNode root{}; init(root); root.mRecordType = Nif::RC_NiNode; root.mRecordIndex = 0;
        root.mFlags = 14;
        Nif::NIFFile file(testNif); file.mVersion = Nif::NIFFile::VER_OB; file.mRoots.push_back(&root);
        auto loaded = Loader::load(file, &mImageManager, &mMaterialManager);
        FindGroupByRecordIndex visitor(0); loaded->accept(visitor); ASSERT_NE(visitor.mFound, nullptr);
        EXPECT_EQ(visitor.mFound->getDataVariance(), osg::Object::STATIC);
        EXPECT_EQ(dynamic_cast<NifOsg::MatrixTransform*>(visitor.mFound), nullptr);
        unsigned flags;
        EXPECT_FALSE(visitor.mFound->getUserValue("nativeNifFlags", flags));
        EXPECT_FALSE(visitor.mFound->getUserValue("nativeNifRecordType", flags));
        EXPECT_FALSE(visitor.mFound->getUserValue("nativeNifLoadedFlags", flags));
    }
    TEST_F(NifOsgLoaderTest, NativeLoadedFlagSnapshotIsDistinctFromControllerRecomputation)
    {
        Nif::NiNode root{}; init(root); root.mRecordType = Nif::RC_NiNode;
        root.mRecordIndex = 0; root.mFlags = 16;
        Nif::bhkCollisionObject collision{}; collision.mRecordType = Nif::RC_bhkCollisionObject;
        collision.mTarget = Nif::NiAVObjectPtr(&root); root.mCollision = Nif::NiCollisionObjectPtr(&collision);
        Nif::NIFFile file(testNif); file.mVersion = Nif::NIFFile::VER_OB; file.mRoots.push_back(&root);
        auto loaded = Loader::load(file, &mImageManager, &mMaterialManager);
        FindGroupByRecordIndex visitor(0); loaded->accept(visitor);
        auto* native = dynamic_cast<NifOsg::MatrixTransform*>(visitor.mFound); ASSERT_NE(native, nullptr);
        unsigned flags = 0;
        ASSERT_TRUE(native->getUserValue("nativeNifLoadedFlags", flags)); EXPECT_EQ(flags, 14);
        SceneUtil::NifSceneTraversal traversal(*native);
        const auto result = traversal.recomputeSelectiveUpdateFlags([&](const auto&) {
            SceneUtil::NifSceneTraversal::SelectiveUpdatePolicy policy;
            policy.mFlags = flags; return policy;
        }, false);
        EXPECT_FALSE(result.mNeedsUpdate); EXPECT_TRUE(result.mRigid);
        EXPECT_EQ(traversal.selectiveUpdateFlags(*native), 16);
        // Recomputing from empty lists at generation would disable projection;
        // it is a separate native event, not initialization from the file.
        ASSERT_TRUE(native->getUserValue("nativeNifLoadedFlags", flags)); EXPECT_EQ(flags, 14);
        ASSERT_TRUE(native->getUserValue("nativeNifFlags", flags)); EXPECT_EQ(flags, 16);
        EXPECT_EQ(root.mFlags, 16u);
    }

}
