#include <apps/openmw/mwrender/oblivioneffectshader.hpp>
#include <apps/openmw/mwworld/oblivionscriptmanager.hpp>
#include <apps/openmw/mwworld/esmstore.hpp>
#include "../mwworld/oblivionworldfixture.hpp"
#include <components/esm4/loadefsh.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/lightmanager.hpp>
#include <components/sceneutil/shadow.hpp>
#include <components/shader/shadermanager.hpp>
#include <components/vfs/manager.hpp>

#include <gtest/gtest.h>
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osgParticle/ParticleSystem>
#include <osgUtil/UpdateVisitor>
#include <limits>

namespace
{
    ESM4::EffectShader fire()
    {
        // Original0852fe, independently decoded in the real-master receipt.
        ESM4::EffectShader shader;
        shader.mParticleTexture = "Fire/FireFlameParticle.dds";
        shader.mData.emplace(); shader.mData->mFlags = 0x11;
        auto& p = shader.mData->mParticles.emplace();
        p.mSourceBlend = 5; p.mBlendOperation = 1; p.mZTest = 4; p.mDestinationBlend = 2;
        p.mBirthFullTime = .6; p.mBirthRampDown = .1;
        p.mBirthFullRatio = 1; p.mBirthPersistentRatio = .9;
        p.mLifetime = .5; p.mLifetimeVariation = .3;
        p.mNormalSpeed = 30; p.mNormalAcceleration = -15; p.mVelocity = {0,0,10};
        p.mScale = {30,36}; p.mScaleTime = {.1,.75};
        p.mColor = {{{255,255,255,0},{255,255,255,0},{173,164,143,0}}};
        p.mAlpha = {1,1,.7}; p.mColorTime = {.01,.5,.8};
        return shader;
    }

    struct FindParticles : osg::NodeVisitor
    {
        osgParticle::ParticleSystem* mSystem = nullptr;
        FindParticles() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}
        void apply(osg::Drawable& drawable) override
        {
            if (auto* system = dynamic_cast<osgParticle::ParticleSystem*>(&drawable)) mSystem = system;
        }
    };

    struct Fixture
    {
        TestingOpenMW::VFSTestFile mTexture{"P6\n1 1\n255\n\xff\xff\xff"};
        VFS::Manager mVfs;
        Resource::ResourceSystem mResources{&mVfs, 0., nullptr};
        osg::ref_ptr<osg::MatrixTransform> mRoot = new osg::MatrixTransform;
        ESM4::EffectShader mShader = fire();
        unsigned int mFrame = 0;
        Fixture()
        {
            mResources.getSceneManager()->setShaderPath(OPENMW_PROJECT_SOURCE_DIR "/files/shaders");
            auto defines = Shader::getDefaultDefines();
            for (const auto& [name,value] : SceneUtil::ShadowManager::getShadowsDisabledDefines())
                defines[name] = value;
            osg::ref_ptr<SceneUtil::LightManager> lights
                = new SceneUtil::LightManager(SceneUtil::LightSettings{}, &mResources);
            for (const auto& [name,value] : lights->getLightDefines()) defines[name] = value;
            mResources.getSceneManager()->getShaderManager().setGlobalDefines(defines);
            mShader.mParticleTexture = "particle.ppm";
            mVfs.addArchive(std::make_unique<TestingOpenMW::VFSTestData>(VFS::FileMap{
                {VFS::Path::Normalized("textures/particle.ppm"), &mTexture}}));
            mVfs.buildIndex();
            osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
            auto vertices = new osg::Vec3Array;
            vertices->push_back({0,0,0}); vertices->push_back({100,0,0}); vertices->push_back({0,100,0});
            geometry->setVertexArray(vertices);
            geometry->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES,0,3));
            osg::ref_ptr<osg::MatrixTransform> local = new osg::MatrixTransform;
            local->setMatrix(osg::Matrix::translate(10,20,30));
            local->addChild(geometry); mRoot->addChild(local);
            mRoot->setMatrix(osg::Matrix::rotate(osg::PI, osg::Vec3(1,0,0))
                * osg::Matrix::translate(1000,2000,3000));
        }
        void advance(double age, double dt)
        {
            MWRender::advanceOblivionParticleShader(*mRoot,"fire",mShader,mResources,age,dt);
            update();
        }
        void update()
        {
            osg::ref_ptr<osg::FrameStamp> stamp = new osg::FrameStamp;
            stamp->setFrameNumber(++mFrame); stamp->setSimulationTime(mFrame * .01);
            osgUtil::UpdateVisitor visitor; visitor.setFrameStamp(stamp);
            mRoot->accept(visitor);
        }
        osgParticle::ParticleSystem* system()
        {
            FindParticles visitor; mRoot->accept(visitor); return visitor.mSystem;
        }
    };

    TEST(OblivionEffectShader, WorldSimulationControlsPauseAndOneEmitterPerIdentity)
    {
        Fixture f; f.advance(.3,.3);
        auto* system = f.system(); ASSERT_NE(system,nullptr);
        ASSERT_GT(system->numParticles() - system->numDeadParticles(),0);
        // Keep the original variable lifetime. Follow a particle that will
        // survive the resumed .1s step; an expired pool slot can be reused by
        // a new particle, whose age correctly restarts at zero.
        int sample = 0;
        while (sample < system->numParticles()
            && (!system->getParticle(sample)->isAlive()
                || system->getParticle(sample)->getLifeTime() - system->getParticle(sample)->getAge() <= .15))
            ++sample;
        ASSERT_LT(sample,system->numParticles());
        const auto position = system->getParticle(sample)->getPosition();
        const auto age = system->getParticle(sample)->getAge();
        for (int i=0; i<5; ++i) f.update(); // Render clock advances, world time does not.
        EXPECT_EQ(system->getParticle(sample)->getPosition(),position);
        EXPECT_EQ(system->getParticle(sample)->getAge(),age);
        f.advance(.4,.1);
        EXPECT_EQ(f.system(),system);
        EXPECT_EQ(f.mRoot->getNumChildren(),2u);
        EXPECT_GT(system->getParticle(sample)->getAge(),age);
    }

    TEST(OblivionEffectShader, SamplesActualSurfaceWithRootAndChildTransformsExactlyOnce)
    {
        Fixture f;
        auto& p = *f.mShader.mData->mParticles;
        p.mVelocity = {}; p.mAcceleration = {}; p.mNormalSpeed = p.mNormalAcceleration = 0;
        f.advance(.3,.3);
        auto* system = f.system(); ASSERT_NE(system,nullptr); ASSERT_GT(system->numParticles(),0);
        int colorMode = 0;
        ASSERT_TRUE(system->getStateSet()->getUniform("colorMode"));
        ASSERT_TRUE(system->getStateSet()->getUniform("colorMode")->get(colorMode));
        EXPECT_EQ(colorMode,2); // GPU path must use particle RGB and separate alpha.
        for (int i=0; i<system->numParticles(); ++i)
        {
            const auto& position = system->getParticle(i)->getPosition();
            EXPECT_NEAR(position.z(),2970,1e-3);
            EXPECT_GE(position.x(),1010); EXPECT_LE(position.x(),1110);
            EXPECT_LE(position.y(),1980); EXPECT_GE(position.y(),1880);
            EXPECT_LE((position.x()-1010) + (1980-position.y()),100.001);
            EXPECT_GT(system->getParticle(i)->getCurrentAlpha(),0); // Raw byte3 is zero.
        }
    }

    TEST(OblivionEffectShader, ColdModelReconstructsActiveCloudAndStopRemovesOnlyItsEffect)
    {
        Fixture f; f.advance(30,0); // Saved indefinite effect, no repeated script initializer.
        ASSERT_NE(f.system(),nullptr);
        EXPECT_GT(f.system()->numParticles() - f.system()->numDeadParticles(),0);
        MWRender::removeOblivionParticleShader(*f.mRoot,"different");
        EXPECT_EQ(f.mRoot->getNumChildren(),2u);
        MWRender::removeOblivionParticleShader(*f.mRoot,"fire");
        EXPECT_EQ(f.system(),nullptr); EXPECT_EQ(f.mRoot->getNumChildren(),1u);
        f.advance(31,0);
        ASSERT_NE(f.system(),nullptr); EXPECT_EQ(f.mRoot->getNumChildren(),2u);
        MWRender::clearOblivionParticleShaders(*f.mRoot);
        EXPECT_EQ(f.system(),nullptr); EXPECT_EQ(f.mRoot->getNumChildren(),1u);
    }

    TEST(OblivionEffectShader, WorldSpaceDrawableCancelsFullNonunitScaleAndRotation)
    {
        Fixture f;
        f.mRoot->setMatrix(osg::Matrix::scale(2,2,2) * f.mRoot->getMatrix());
        f.advance(.2,.2);
        auto* system = f.system(); ASSERT_NE(system,nullptr);
        const auto matrices = system->getWorldMatrices(); ASSERT_EQ(matrices.size(),1u);
        const osg::Vec3 point(123,456,789);
        EXPECT_LT((point * matrices[0] - point).length(),1e-3);
    }

    TEST(OblivionEffectShader, RejectsUnsupportedVisualsAndInvalidNumericalDataBeforeAttachment)
    {
        auto shader = fire(); EXPECT_NO_THROW(MWRender::validateOblivionParticleShader(shader));
        shader.mData->mFlags = 0;
        EXPECT_THROW(MWRender::validateOblivionParticleShader(shader),std::invalid_argument);
        shader = fire(); shader.mData->mParticles->mLifetime = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(MWRender::validateOblivionParticleShader(shader),std::invalid_argument);
        shader = fire(); shader.mData->mParticles->mSourceBlend = 99;
        EXPECT_THROW(MWRender::validateOblivionParticleShader(shader),std::invalid_argument);
    }

    TEST(OblivionEffectShader, RestoreValidatesPairedStableIdentityAndDurationBeforeReplacingRuntime)
    {
        TestingOpenMW::NativeWorldFixture fixture;
        auto shader = fire();
        shader.mId = ESM::FormId{0x900,0};
        shader.mFormKey = ESM::FormKey::content("headless.esm",0x900);
        fixture.mWorld.getStore().getWritable<ESM4::EffectShader>().insertStatic(shader,shader.mFormKey);
        MWWorld::OblivionScriptManager host(fixture.mWorld,fixture.mWorld.getStore(),{"headless.esm"});
        ESM4::RuntimeState saved;
        saved.mScriptEventSequence = 37;
        ESM4::RuntimeReferenceState reference;
        reference.mKey = ESM::FormKey::content("headless.esm",0xa00);
        const auto age = "obscript.shader.age." + shader.mFormKey.serialize();
        const auto duration = "obscript.shader.duration." + shader.mFormKey.serialize();
        reference.mCustomState[age] = 27.5;
        reference.mCustomState[duration] = -1.0;
        saved.mReferences.push_back(reference);
        auto accepted = host.prepareRestore(saved); ASSERT_TRUE(accepted.commit());
        const auto unchanged = [&] {
            ESM4::RuntimeState result; host.capture(result); EXPECT_EQ(result.mScriptEventSequence,37u);
        };
        for (const ESM4::RuntimeValue& invalid : {ESM4::RuntimeValue(-2.0), ESM4::RuntimeValue(true),
                 ESM4::RuntimeValue(std::numeric_limits<double>::infinity())})
        {
            auto bad = saved; bad.mScriptEventSequence = 99;
            bad.mReferences[0].mCustomState[duration] = invalid;
            EXPECT_THROW(host.prepareRestore(bad),std::invalid_argument); unchanged();
        }
        auto orphan = saved; orphan.mReferences[0].mCustomState.erase(age);
        EXPECT_THROW(host.prepareRestore(orphan),std::invalid_argument); unchanged();
        auto missing = saved;
        missing.mReferences[0].mCustomState["obscript.shader.age.content:missing.esm:000900"] = 0.0;
        EXPECT_THROW(host.prepareRestore(missing),std::invalid_argument); unchanged();
        auto finite = saved; finite.mReferences[0].mCustomState[duration] = .5;
        EXPECT_NO_THROW(host.prepareRestore(finite));
    }
}
