#include "oblivioneffectshader.hpp"
#include "vismask.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

#include <osg/BlendEquation>
#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/Material>
#include <osg/NodeCallback>
#include <osg/Texture2D>
#include <osg/TriangleFunctor>

#include <components/esm4/loadefsh.hpp>
#include <components/nifosg/particle.hpp>
#include <components/resource/imagemanager.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/morphgeometry.hpp>
#include <components/sceneutil/riggeometry.hpp>
#include <components/vfs/manager.hpp>

namespace MWRender
{
    namespace
    {
        constexpr std::string_view Prefix = "TES4 EFSH ";

        // EFSH uses the Direct3D9 blend/comparison enumeration values.
        GLenum blend(std::uint32_t value)
        {
            static constexpr GLenum values[] = {0, GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
                GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA,
                GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA_SATURATE};
            if (value == 0 || value >= std::size(values))
                throw std::invalid_argument("Unsupported TES4 particle blend factor");
            return values[value];
        }

        osg::BlendEquation::Equation equation(std::uint32_t value)
        {
            switch (value)
            {
                case 1: return osg::BlendEquation::FUNC_ADD;
                case 2: return osg::BlendEquation::FUNC_SUBTRACT;
                case 3: return osg::BlendEquation::FUNC_REVERSE_SUBTRACT;
                case 4: return osg::BlendEquation::RGBA_MIN;
                case 5: return osg::BlendEquation::RGBA_MAX;
                default: throw std::invalid_argument("Unsupported TES4 particle blend operation");
            }
        }

        osg::Depth::Function comparison(std::uint32_t value)
        {
            static constexpr osg::Depth::Function values[] = {osg::Depth::NEVER, osg::Depth::NEVER,
                osg::Depth::LESS, osg::Depth::EQUAL, osg::Depth::LEQUAL, osg::Depth::GREATER,
                osg::Depth::NOTEQUAL, osg::Depth::GEQUAL, osg::Depth::ALWAYS};
            if (value == 0 || value >= std::size(values))
                throw std::invalid_argument("Unsupported TES4 particle depth comparison");
            return values[value];
        }

        struct Triangle
        {
            osg::Vec3 mA, mB, mC;
            double mCumulativeArea;
        };

        struct CollectTriangles
        {
            std::vector<Triangle>* mTriangles = nullptr;
            osg::Matrix mTransform;
            void operator()(const osg::Vec3& a, const osg::Vec3& b, const osg::Vec3& c)
            {
                const osg::Vec3 x = a * mTransform, y = b * mTransform, z = c * mTransform;
                const double area = ((y - x) ^ (z - x)).length() * .5;
                if (std::isfinite(area) && area > 1e-6)
                    mTriangles->push_back({x, y, z, area + (mTriangles->empty() ? 0 : mTriangles->back().mCumulativeArea)});
            }
        };

        class Surface : public osg::NodeVisitor
        {
        public:
            std::vector<Triangle> mTriangles;
            Surface() : osg::NodeVisitor(TRAVERSE_ACTIVE_CHILDREN) {}
            void apply(osg::Node& node) override
            {
                if (!node.getName().starts_with(Prefix)) traverse(node);
            }
            void apply(osg::Drawable& drawable) override
            {
                if (dynamic_cast<SceneUtil::RigGeometry*>(&drawable)
                    || dynamic_cast<SceneUtil::MorphGeometry*>(&drawable))
                    throw std::invalid_argument("TES4 particle surface currently requires rigid geometry");
                if (!drawable.asGeometry()) return;
                osg::TriangleFunctor<CollectTriangles> collect;
                collect.mTriangles = &mTriangles;
                // The instance root's own transform is already in the later
                // update path. Collect only its descendants in model space.
                const auto& path = getNodePath();
                collect.mTransform = osg::computeLocalToWorld(osg::NodePath(path.begin() + 1, path.end()));
                drawable.accept(collect);
            }
        };

        float interpolate(float time, float aTime, float bTime, float a, float b)
        {
            if (bTime <= aTime) return time < bTime ? a : b;
            return a + (b - a) * std::clamp((time - aTime) / (bTime - aTime), 0.f, 1.f);
        }

        class Particles final : public osg::NodeCallback
        {
        public:
            ESM4::EffectShaderParticles mData;
            std::vector<Triangle> mTriangles;
            osg::ref_ptr<NifOsg::ParticleSystem> mSystem = new NifOsg::ParticleSystem;
            osg::ref_ptr<osg::MatrixTransform> mWorldSpace = new osg::MatrixTransform;
            std::vector<osg::Vec3> mAccelerations;
            std::minstd_rand mRandom{std::random_device{}()}; // Cosmetic RNG, separate from ObScript.
            double mPending = 0, mElapsed, mBirthRemainder = 0;
            bool mWarm = true;

            Particles(const ESM4::EffectShaderParticles& data, std::vector<Triangle> triangles, double elapsed)
                : mData(data), mTriangles(std::move(triangles)), mElapsed(elapsed)
            {
                mSystem->setQuota(512);
                mSystem->setNodeMask(Mask_ParticleSystem);
                mSystem->setSortMode(osgParticle::ParticleSystem::SORT_BACK_TO_FRONT);
                mSystem->getOrCreateUserDataContainer()->addDescription("worldspace");
                mSystem->setParticleScaleReferenceFrame(osgParticle::ParticleSystem::WORLD_COORDINATES);
                mWorldSpace->addChild(mSystem);
            }

            double random() { return std::generate_canonical<double, 24>(mRandom); }

            void emit(const osg::Matrix& world)
            {
                const double pick = random() * mTriangles.back().mCumulativeArea;
                const auto& triangle = *std::lower_bound(mTriangles.begin(), mTriangles.end(), pick,
                    [](const Triangle& t, double area) { return t.mCumulativeArea < area; });
                const double u = std::sqrt(random()), v = random();
                const osg::Vec3 a = triangle.mA * world, b = triangle.mB * world, c = triangle.mC * world;
                osg::Vec3 normal = (b - a) ^ (c - a);
                normal.normalize();
                osgParticle::Particle particle;
                particle.setLifeTime(std::max(.01, mData.mLifetime + (random() * 2 - 1) * mData.mLifetimeVariation));
                particle.setPosition(a * (1 - u) + b * (u * (1 - v)) + c * (u * v));
                particle.setVelocity(normal * mData.mNormalSpeed
                    + osg::Vec3(mData.mVelocity[0], mData.mVelocity[1], mData.mVelocity[2]));
                if (auto* created = mSystem->createParticle(&particle))
                {
                    const auto index = created - mSystem->getParticle(0);
                    mAccelerations.resize(mSystem->numParticles());
                    mAccelerations[index] = normal * mData.mNormalAcceleration
                        + osg::Vec3(mData.mAcceleration[0], mData.mAcceleration[1], mData.mAcceleration[2]);
                }
            }

            void step(double dt, const osg::Matrix& world, osg::NodeVisitor& visitor)
            {
                mElapsed += dt;
                float ratio = mData.mBirthPersistentRatio;
                if (mElapsed < mData.mBirthRampUp)
                    ratio = interpolate(mElapsed, 0, mData.mBirthRampUp, 0, mData.mBirthFullRatio);
                else if (mElapsed < mData.mBirthRampUp + mData.mBirthFullTime)
                    ratio = mData.mBirthFullRatio;
                else if (mElapsed < mData.mBirthRampUp + mData.mBirthFullTime + mData.mBirthRampDown)
                    ratio = interpolate(mElapsed, mData.mBirthRampUp + mData.mBirthFullTime,
                        mData.mBirthRampUp + mData.mBirthFullTime + mData.mBirthRampDown,
                        mData.mBirthFullRatio, mData.mBirthPersistentRatio);
                // Area-based cosmetic density. TES4's exact particle quota/RNG
                // is not reproduced; authored time, scale, color and motion are.
                mBirthRemainder += dt * std::min(400.0, mTriangles.back().mCumulativeArea / 100) * std::max(0.f, ratio);
                while (mBirthRemainder >= 1) { emit(world); --mBirthRemainder; }
                for (int i = 0; i < mSystem->numParticles(); ++i)
                {
                    auto* particle = mSystem->getParticle(i);
                    if (!particle->isAlive()) continue;
                    const float time = (particle->getAge() + dt) / particle->getLifeTime();
                    const float size = interpolate(time, mData.mScaleTime[0], mData.mScaleTime[1],
                        mData.mScale[0], mData.mScale[1]);
                    particle->setSizeRange({size, size});
                    const int key = time < mData.mColorTime[1] ? 0 : 1;
                    osg::Vec4 color;
                    for (int channel = 0; channel < 3; ++channel)
                        color[channel] = interpolate(time, mData.mColorTime[key], mData.mColorTime[key+1],
                            mData.mColor[key][channel] / 255.f, mData.mColor[key+1][channel] / 255.f);
                    color.a() = 1; // Color byte3 is padding, not the separate particle alpha envelope.
                    const float alpha = interpolate(time, mData.mColorTime[key], mData.mColorTime[key+1],
                        mData.mAlpha[key], mData.mAlpha[key+1]);
                    particle->setColorRange({color, color});
                    particle->setAlphaRange({alpha, alpha});
                    particle->addVelocity(mAccelerations[i] * dt);
                }
                mSystem->update(dt, visitor);
            }

            void operator()(osg::Node* node, osg::NodeVisitor* visitor) override
            {
                const osg::Matrix world = osg::computeLocalToWorld(visitor->getNodePath());
                // Positions already include the full instance scale. The NIF
                // inverse callback deliberately retains scale and would apply
                // it again to these world-space positions.
                mWorldSpace->setMatrix(osg::Matrix::inverse(world));
                double dt = std::exchange(mPending, 0.0);
                if (mWarm)
                {
                    // Reconstruct a steady-state cloud on cold attachment;
                    // preserve its saved envelope phase, not individual RNG.
                    const double warm = std::min(mElapsed, double(mData.mLifetime + mData.mLifetimeVariation));
                    mElapsed -= warm;
                    dt += warm;
                    mWarm = false;
                }
                while (dt > 0)
                {
                    const double stepTime = std::min(dt, 1.0 / 60);
                    step(stepTime, world, *visitor);
                    dt -= stepTime;
                }
                node->setUserValue("tes4ParticleCount", mSystem->numParticles() - mSystem->numDeadParticles());
                traverse(node, visitor);
            }
        };
    }

    void validateOblivionParticleShader(const ESM4::EffectShader& shader)
    {
        if (!shader.mData || !(shader.mData->mFlags & ESM4::EffectShaderData::NoMembrane)
            || (shader.mData->mFlags & (ESM4::EffectShaderData::NoParticles | ESM4::EffectShaderData::SkinOnly))
            || !shader.mData->mParticles || shader.mParticleTexture.empty())
            throw std::invalid_argument("TES4 visual currently requires a particle-only EFSH");
        const auto& p = *shader.mData->mParticles;
        blend(p.mSourceBlend); blend(p.mDestinationBlend); equation(p.mBlendOperation); comparison(p.mZTest);
        const auto finite = [](auto&& values) {
            return std::all_of(std::begin(values), std::end(values), [](float v) { return std::isfinite(v); });
        };
        const float scalars[] = {p.mBirthRampUp, p.mBirthFullTime, p.mBirthRampDown, p.mBirthFullRatio,
            p.mBirthPersistentRatio, p.mLifetime, p.mLifetimeVariation, p.mNormalSpeed, p.mNormalAcceleration};
        if (!finite(scalars) || !finite(p.mVelocity) || !finite(p.mAcceleration) || !finite(p.mScale)
            || !finite(p.mScaleTime) || !finite(p.mAlpha) || !finite(p.mColorTime)
            || p.mLifetime <= 0 || p.mLifetimeVariation < 0 || p.mBirthRampUp < 0
            || p.mBirthFullTime < 0 || p.mBirthRampDown < 0 || p.mBirthFullRatio < 0
            || p.mBirthPersistentRatio < 0 || p.mScale[0] < 0 || p.mScale[1] < 0
            || p.mColorTime[0] > p.mColorTime[1] || p.mColorTime[1] > p.mColorTime[2]
            || p.mScaleTime[0] > p.mScaleTime[1])
            throw std::invalid_argument("Invalid TES4 particle shader envelope");
    }

    void advanceOblivionParticleShader(osg::Group& model, std::string_view identity,
        const ESM4::EffectShader& shader, Resource::ResourceSystem& resources, double elapsed, double seconds)
    {
        const std::string name = std::string(Prefix) + std::string(identity);
        for (unsigned int i = 0; i < model.getNumChildren(); ++i)
            if (auto* child = model.getChild(i); child->getName() == name)
            {
                auto* particles = dynamic_cast<Particles*>(child->getUpdateCallback());
                if (!particles) throw std::logic_error("TES4 particle instance lost its controller");
                particles->mPending += std::max(0.0, seconds);
                return;
            }
        validateOblivionParticleShader(shader);
        Surface surface;
        model.accept(surface);
        if (surface.mTriangles.empty()) return;
        const auto& p = *shader.mData->mParticles;
        const VFS::Path::Normalized texturePath("textures/" + shader.mParticleTexture);
        if (!resources.getVFS()->exists(texturePath))
            throw std::runtime_error("Missing authored TES4 particle texture: " + texturePath.value());
        const auto image = resources.getImageManager()->getImage(texturePath);
        if (image == resources.getImageManager()->getWarningImage())
            throw std::runtime_error("Failed to decode authored TES4 particle texture: " + texturePath.value());
        osg::ref_ptr<Particles> particles = new Particles(p, std::move(surface.mTriangles), std::max(0.0, elapsed - seconds));
        particles->mPending = std::max(0.0, seconds);
        osg::ref_ptr<osg::Group> effect = new osg::Group;
        effect->setName(name);
        effect->setNodeMask(Mask_Effect);
        effect->setUpdateCallback(particles);
        effect->addChild(particles->mWorldSpace);
        auto* state = particles->mSystem->getOrCreateStateSet();
        osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D(image);
        texture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        texture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        state->setTextureAttributeAndModes(0, texture, osg::StateAttribute::ON);
        state->setAttributeAndModes(new osg::BlendFunc(blend(p.mSourceBlend), blend(p.mDestinationBlend)));
        state->setAttributeAndModes(new osg::BlendEquation(equation(p.mBlendOperation)));
        state->setAttributeAndModes(new osg::Depth(comparison(p.mZTest), 0, 1, false));
        // EFSH fire is self-lit. The ordinary object shader ignores
        // GL_LIGHTING and would also lose the particle's alpha without a
        // vertex-color material. Reuse the existing unlit texture pipeline.
        osg::ref_ptr<osg::Material> material = new osg::Material;
        material->setColorMode(osg::Material::AMBIENT_AND_DIFFUSE);
        state->setAttributeAndModes(material);
        state->addUniform(new osg::Uniform("useFalloff", false));
        particles->mSystem->setUserValue("shaderPrefix", std::string("bs/nolighting"));
        state->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
        resources.getSceneManager()->recreateShaders(effect, "bs/nolighting");
        model.addChild(effect);
    }

    void removeOblivionParticleShader(osg::Group& model, std::string_view identity)
    {
        const std::string name = std::string(Prefix) + std::string(identity);
        for (unsigned int i = 0; i < model.getNumChildren();)
            if (model.getChild(i)->getName() == name) model.removeChild(i);
            else ++i;
    }

    OblivionParticleShaderStats inspectOblivionParticleShader(const osg::Group& model, std::string_view identity)
    {
        OblivionParticleShaderStats result;
        const std::string name = std::string(Prefix) + std::string(identity);
        for (unsigned int i = 0; i < model.getNumChildren(); ++i)
            if (const auto* child = model.getChild(i); child->getName() == name)
            {
                ++result.mInstances;
                if (const auto* particles = dynamic_cast<const Particles*>(child->getUpdateCallback()))
                {
                    result.mParticles += particles->mSystem->numParticles() - particles->mSystem->numDeadParticles();
                    result.mSurfaceTriangles += particles->mTriangles.size();
                }
            }
        return result;
    }

    void clearOblivionParticleShaders(osg::Group& model)
    {
        for (unsigned int i = 0; i < model.getNumChildren();)
            if (model.getChild(i)->getName().starts_with(Prefix)) model.removeChild(i);
            else ++i;
    }
}
