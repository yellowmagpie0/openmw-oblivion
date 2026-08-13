#include "../nif/node.hpp"

#include <components/nif/node.hpp>
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
