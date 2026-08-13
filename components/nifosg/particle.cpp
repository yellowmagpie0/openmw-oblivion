#include "particle.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/ValueObject>
#include <osgParticle/ConstantRateCounter>

#include <components/debug/debuglog.hpp>
#include <components/misc/rng.hpp>
#include <components/nif/data.hpp>
#include <components/sceneutil/morphgeometry.hpp>
#include <components/sceneutil/riggeometry.hpp>

namespace
{
    class FindFirstGeometry : public osg::NodeVisitor
    {
    public:
        FindFirstGeometry()
            : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
            , mGeometry(nullptr)
        {
        }

        void apply(osg::Node& node) override
        {
            if (mGeometry)
                return;

            traverse(node);
        }

        void apply(osg::Drawable& drawable) override
        {
            if (auto morph = dynamic_cast<SceneUtil::MorphGeometry*>(&drawable))
            {
                mGeometry = morph->getSourceGeometry();
                return;
            }
            else if (auto rig = dynamic_cast<SceneUtil::RigGeometry*>(&drawable))
            {
                mGeometry = rig->getSourceGeometry();
                return;
            }

            traverse(drawable);
        }

        void apply(osg::Geometry& geometry) override { mGeometry = &geometry; }

        osg::Geometry* mGeometry;
    };

    class LocalToWorldAccumulator : public osg::NodeVisitor
    {
    public:
        LocalToWorldAccumulator(osg::Matrix& matrix)
            : osg::NodeVisitor()
            , mMatrix(matrix)
        {
        }

        virtual void apply(osg::Transform& transform)
        {
            if (&transform != mLastAppliedTransform)
            {
                mLastAppliedTransform = &transform;
                mLastMatrix = mMatrix;
            }
            transform.computeLocalToWorldMatrix(mMatrix, this);
        }

        void accumulate(const osg::NodePath& path)
        {
            if (path.empty())
                return;

            size_t i = path.size();

            for (auto rit = path.rbegin(); rit != path.rend(); rit++, --i)
            {
                const osg::Camera* camera = (*rit)->asCamera();
                if (camera
                    && (camera->getReferenceFrame() != osg::Transform::RELATIVE_RF || camera->getParents().empty()))
                    break;
            }

            for (; i < path.size(); ++i)
                path[i]->accept(*this);
        }

        osg::Matrix& mMatrix;
        std::optional<osg::Matrix> mLastMatrix;
        osg::Transform* mLastAppliedTransform = nullptr;
    };
}

namespace NifOsg
{

    VolumePlacer::VolumePlacer(Shape shape, const osg::Vec3f& extents)
        : mShape(shape)
        , mExtents(extents)
    {
    }

    VolumePlacer::VolumePlacer(const VolumePlacer& copy, const osg::CopyOp& copyop)
        : osgParticle::Placer(copy, copyop)
        , mShape(copy.mShape)
        , mExtents(copy.mExtents)
    {
    }

    void VolumePlacer::place(osgParticle::Particle* particle) const
    {
        osg::Vec3f position;
        if (mShape == Shape::Box)
        {
            position.set((Misc::Rng::rollClosedProbability() * 2.f - 1.f) * mExtents.x(),
                (Misc::Rng::rollClosedProbability() * 2.f - 1.f) * mExtents.y(),
                (Misc::Rng::rollClosedProbability() * 2.f - 1.f) * mExtents.z());
        }
        else
        {
            const float angle = Misc::Rng::rollClosedProbability() * osg::PI * 2.f;
            const float radius = std::sqrt(Misc::Rng::rollClosedProbability()) * mExtents.x();
            position.set(std::cos(angle) * radius, std::sin(angle) * radius,
                (Misc::Rng::rollClosedProbability() * 2.f - 1.f) * mExtents.z());
            if (mShape == Shape::Sphere)
            {
                do
                {
                    position.set(Misc::Rng::rollClosedProbability() * 2.f - 1.f,
                        Misc::Rng::rollClosedProbability() * 2.f - 1.f,
                        Misc::Rng::rollClosedProbability() * 2.f - 1.f);
                } while (position.length2() > 1.f);
                position *= mExtents.x();
            }
        }
        particle->setPosition(position);
    }

    ParticleSystem::ParticleSystem()
        : osgParticle::ParticleSystem()
        , mQuota(std::numeric_limits<int>::max())
    {
        mNormalArray = new osg::Vec3Array(1);
        mNormalArray->setBinding(osg::Array::BIND_OVERALL);
        (*mNormalArray.get())[0] = osg::Vec3(0.3f, 0.3f, 0.3f);
    }

    ParticleSystem::ParticleSystem(const ParticleSystem& copy, const osg::CopyOp& copyop)
        : osgParticle::ParticleSystem(copy, copyop)
        , mQuota(copy.mQuota)
    {
        mNormalArray = new osg::Vec3Array(1);
        mNormalArray->setBinding(osg::Array::BIND_OVERALL);
        (*mNormalArray.get())[0] = osg::Vec3(0.3f, 0.3f, 0.3f);

        // For some reason the osgParticle constructor doesn't copy the particles
        for (int i = 0; i < copy.numParticles() - copy.numDeadParticles(); ++i)
            ParticleSystem::createParticle(copy.getParticle(i));
    }

    void ParticleSystem::setQuota(int quota)
    {
        mQuota = quota;
    }

    osgParticle::Particle* ParticleSystem::createParticle(const osgParticle::Particle* ptemplate)
    {
        if (numParticles() - numDeadParticles() < mQuota)
            return osgParticle::ParticleSystem::createParticle(ptemplate);
        return nullptr;
    }

    void ParticleSystem::drawImplementation(osg::RenderInfo& renderInfo) const
    {
        osg::State& state = *renderInfo.getState();
        if (state.useVertexArrayObject(getUseVertexArrayObject()))
        {
            state.getCurrentVertexArrayState()->assignNormalArrayDispatcher();
            state.getCurrentVertexArrayState()->setNormalArray(state, mNormalArray);
        }
        else
        {
            state.getAttributeDispatchers().activateNormalArray(mNormalArray);
        }
        osgParticle::ParticleSystem::drawImplementation(renderInfo);
    }

    void InverseWorldMatrix::operator()(osg::MatrixTransform* node, osg::NodeVisitor* nv)
    {
        osg::NodePath path = nv->getNodePath();
        path.pop_back();

        osg::Matrix mat = osg::computeLocalToWorld(path);
        mat.orthoNormalize(mat); // don't undo the scale
        mat.invert(mat);
        node->setMatrix(mat);

        traverse(node, nv);
    }

    ParticleShooter::ParticleShooter(float minSpeed, float maxSpeed, float horizontalDir, float horizontalAngle,
        float verticalDir, float verticalAngle, float lifetime, float lifetimeRandom)
        : mMinSpeed(minSpeed)
        , mMaxSpeed(maxSpeed)
        , mHorizontalDir(horizontalDir)
        , mHorizontalAngle(horizontalAngle)
        , mVerticalDir(verticalDir)
        , mVerticalAngle(verticalAngle)
        , mLifetime(lifetime)
        , mLifetimeRandom(lifetimeRandom)
    {
    }

    ParticleShooter::ParticleShooter()
        : mMinSpeed(0.f)
        , mMaxSpeed(0.f)
        , mHorizontalDir(0.f)
        , mHorizontalAngle(0.f)
        , mVerticalDir(0.f)
        , mVerticalAngle(0.f)
        , mLifetime(0.f)
        , mLifetimeRandom(0.f)
    {
    }

    ParticleShooter::ParticleShooter(const ParticleShooter& copy, const osg::CopyOp& copyop)
        : osgParticle::Shooter(copy, copyop)
    {
        mMinSpeed = copy.mMinSpeed;
        mMaxSpeed = copy.mMaxSpeed;
        mHorizontalDir = copy.mHorizontalDir;
        mHorizontalAngle = copy.mHorizontalAngle;
        mVerticalDir = copy.mVerticalDir;
        mVerticalAngle = copy.mVerticalAngle;
        mLifetime = copy.mLifetime;
        mLifetimeRandom = copy.mLifetimeRandom;
        mRadius = copy.mRadius;
        mRadiusVariation = copy.mRadiusVariation;
        mRotationSpeed = copy.mRotationSpeed;
        mRotationSpeedVariation = copy.mRotationSpeedVariation;
        mRotationAngle = copy.mRotationAngle;
        mRotationAngleVariation = copy.mRotationAngleVariation;
        mRandomRotationSpeedSign = copy.mRandomRotationSpeedSign;
        mRandomRotationAxis = copy.mRandomRotationAxis;
        mRotationAxis = copy.mRotationAxis;
    }

    void ParticleShooter::shoot(osgParticle::Particle* particle) const
    {
        float hdir = mHorizontalDir + mHorizontalAngle * (2.f * Misc::Rng::rollClosedProbability() - 1.f);
        float vdir = mVerticalDir + mVerticalAngle * (2.f * Misc::Rng::rollClosedProbability() - 1.f);

        osg::Vec3f dir
            = (osg::Quat(vdir, osg::Vec3f(0, 1, 0)) * osg::Quat(hdir, osg::Vec3f(0, 0, 1))) * osg::Vec3f(0, 0, 1);

        float vel = mMinSpeed + (mMaxSpeed - mMinSpeed) * Misc::Rng::rollClosedProbability();
        particle->setVelocity(dir * vel);

        const float radius
            = std::max(0.f, mRadius + mRadiusVariation * (2.f * Misc::Rng::rollClosedProbability() - 1.f));
        particle->setSizeRange(osgParticle::rangef(radius, radius));
        particle->setRadius(radius);

        osg::Vec3f rotationAxis = mRotationAxis;
        if (mRandomRotationAxis)
        {
            do
            {
                rotationAxis.set(Misc::Rng::rollClosedProbability() * 2.f - 1.f,
                    Misc::Rng::rollClosedProbability() * 2.f - 1.f,
                    Misc::Rng::rollClosedProbability() * 2.f - 1.f);
            } while (rotationAxis.length2() < 1e-6f || rotationAxis.length2() > 1.f);
        }
        rotationAxis.normalize();
        const float rotationAngle
            = mRotationAngle + mRotationAngleVariation * (2.f * Misc::Rng::rollClosedProbability() - 1.f);
        float rotationSpeed
            = mRotationSpeed + mRotationSpeedVariation * (2.f * Misc::Rng::rollClosedProbability() - 1.f);
        if (mRandomRotationSpeedSign && Misc::Rng::rollClosedProbability() < .5f)
            rotationSpeed = -rotationSpeed;
        particle->setAngle(rotationAxis * rotationAngle);
        particle->setAngularVelocity(rotationAxis * rotationSpeed);

        // Not supposed to set this here, but there doesn't seem to be a better way of doing it
        particle->setLifeTime(std::max(
            std::numeric_limits<float>::epsilon(), mLifetime + mLifetimeRandom * Misc::Rng::rollClosedProbability()));
    }

    void ParticleShooter::setSpeed(float speed, float variation)
    {
        mMinSpeed = speed - variation * 0.5f;
        mMaxSpeed = speed + variation * 0.5f;
    }

    void ParticleShooter::setAngles(
        float declination, float declinationVariation, float planarAngle, float planarAngleVariation)
    {
        mVerticalDir = declination;
        mVerticalAngle = declinationVariation;
        mHorizontalDir = planarAngle;
        mHorizontalAngle = planarAngleVariation;
    }

    void ParticleShooter::setLifetime(float lifetime, float variation)
    {
        mLifetime = lifetime;
        mLifetimeRandom = variation;
    }

    void ParticleShooter::setRadius(float radius, float variation)
    {
        mRadius = radius;
        mRadiusVariation = variation;
    }

    void ParticleShooter::setRotation(const Nif::NiPSysRotationModifier* rotation)
    {
        if (rotation == nullptr)
            return;
        mRotationSpeed = rotation->mRotationSpeed;
        mRotationSpeedVariation = rotation->mRotationSpeedVariation;
        mRotationAngle = rotation->mRotationAngle;
        mRotationAngleVariation = rotation->mRotationAngleVariation;
        mRandomRotationSpeedSign = rotation->mRandomRotSpeedSign;
        mRandomRotationAxis = rotation->mRandomAxis;
        mRotationAxis = rotation->mAxis;
    }

    GrowFadeAffector::GrowFadeAffector(float growTime, float fadeTime)
        : mGrowTime(growTime)
        , mFadeTime(fadeTime)
        , mCachedDefaultSize(0.f)
    {
    }

    GrowFadeAffector::GrowFadeAffector()
        : mGrowTime(0.f)
        , mFadeTime(0.f)
        , mCachedDefaultSize(0.f)
    {
    }

    GrowFadeAffector::GrowFadeAffector(const GrowFadeAffector& copy, const osg::CopyOp& copyop)
        : osgParticle::Operator(copy, copyop)
    {
        mGrowTime = copy.mGrowTime;
        mFadeTime = copy.mFadeTime;
        mCachedDefaultSize = copy.mCachedDefaultSize;
    }

    void GrowFadeAffector::beginOperate(osgParticle::Program* program)
    {
        mCachedDefaultSize = program->getParticleSystem()->getDefaultParticleTemplate().getSizeRange().minimum;
    }

    void GrowFadeAffector::operate(osgParticle::Particle* particle, double /* dt */)
    {
        float size = mCachedDefaultSize;
        if (particle->getAge() < mGrowTime && mGrowTime != 0.f)
            size *= static_cast<float>(particle->getAge() / mGrowTime);
        if (particle->getLifeTime() - particle->getAge() < mFadeTime && mFadeTime != 0.f)
            size *= static_cast<float>(particle->getLifeTime() - particle->getAge()) / mFadeTime;
        particle->setSizeRange(osgParticle::rangef(size, size));
    }

    ParticleColorAffector::ParticleColorAffector(const Nif::NiColorData* clrdata)
        : mData(clrdata->mKeyMap, osg::Vec4f(1, 1, 1, 1))
    {
    }

    ParticleColorAffector::ParticleColorAffector() {}

    ParticleColorAffector::ParticleColorAffector(const ParticleColorAffector& copy, const osg::CopyOp& copyop)
        : osgParticle::Operator(copy, copyop)
    {
        mData = copy.mData;
    }

    void ParticleColorAffector::operate(osgParticle::Particle* particle, double /* dt */)
    {
        assert(particle->getLifeTime() > 0);
        float time = static_cast<float>(particle->getAge() / particle->getLifeTime());
        osg::Vec4f color = mData.interpKey(time);
        float alpha = color.a();
        color.a() = 1.0f;

        particle->setColorRange(osgParticle::rangev4(color, color));
        particle->setAlphaRange(osgParticle::rangef(alpha, alpha));
    }

    GravityAffector::GravityAffector(const Nif::NiGravity* gravity)
        : mForce(gravity->mForce)
        , mType(gravity->mType)
        , mPosition(gravity->mPosition)
        , mDirection(gravity->mDirection)
        , mDecay(gravity->mDecay)
    {
    }

    GravityAffector::GravityAffector(const GravityAffector& copy, const osg::CopyOp& copyop)
        : osgParticle::Operator(copy, copyop)
    {
        mForce = copy.mForce;
        mType = copy.mType;
        mPosition = copy.mPosition;
        mDirection = copy.mDirection;
        mDecay = copy.mDecay;
        mCachedWorldPosition = copy.mCachedWorldPosition;
        mCachedWorldDirection = copy.mCachedWorldDirection;
    }

    void GravityAffector::beginOperate(osgParticle::Program* program)
    {
        bool absolute = (program->getReferenceFrame() == osgParticle::ParticleProcessor::ABSOLUTE_RF);

        // We don't need the position for Wind gravity, except if decay is being applied
        if (mType == Nif::ForceType::Point || mDecay != 0.f)
            mCachedWorldPosition = absolute ? program->transformLocalToWorld(mPosition) : mPosition;

        mCachedWorldDirection = absolute ? program->rotateLocalToWorld(mDirection) : mDirection;
        mCachedWorldDirection.normalize();
    }

    void GravityAffector::operate(osgParticle::Particle* particle, double dt)
    {
        const float magic = 1.6f;
        switch (mType)
        {
            case Nif::ForceType::Wind:
            {
                float decayFactor = 1.f;
                if (mDecay != 0.f)
                {
                    osg::Plane gravityPlane(mCachedWorldDirection, mCachedWorldPosition);
                    float distance = std::abs(gravityPlane.distance(particle->getPosition()));
                    decayFactor = std::exp(-1.f * mDecay * distance);
                }

                particle->addVelocity(mCachedWorldDirection * mForce * static_cast<float>(dt) * decayFactor * magic);

                break;
            }
            case Nif::ForceType::Point:
            {
                osg::Vec3f diff = mCachedWorldPosition - particle->getPosition();

                float decayFactor = 1.f;
                if (mDecay != 0.f)
                    decayFactor = std::exp(-1.f * mDecay * diff.length());

                diff.normalize();

                particle->addVelocity(diff * mForce * static_cast<float>(dt) * decayFactor * magic);
                break;
            }
        }
    }

    ParticleBomb::ParticleBomb(const Nif::NiParticleBomb* bomb)
        : mRange(bomb->mRange)
        , mStrength(bomb->mStrength)
        , mDecayType(bomb->mDecayType)
        , mSymmetryType(bomb->mSymmetryType)
        , mPosition(bomb->mPosition)
        , mDirection(bomb->mDirection)
    {
    }

    ParticleBomb::ParticleBomb(const Nif::NiPSysBombModifier* bomb)
        : mRange(bomb->mRange)
        , mStrength(bomb->mStrength)
        , mDecayType(bomb->mDecayType)
        , mSymmetryType(bomb->mSymmetryType)
        , mDirection(bomb->mBombAxis)
        , mBombObjectRecordIndex(bomb->mBombObject.empty() ? -1 : bomb->mBombObject->mRecordIndex)
    {
        if (!bomb->mBombObject.empty())
            mPosition = bomb->mBombObject->mTransform.mTranslation;
    }

    ParticleBomb::ParticleBomb(const ParticleBomb& copy, const osg::CopyOp& copyop)
        : osgParticle::Operator(copy, copyop)
    {
        mRange = copy.mRange;
        mStrength = copy.mStrength;
        mDecayType = copy.mDecayType;
        mSymmetryType = copy.mSymmetryType;
        mCachedWorldPosition = copy.mCachedWorldPosition;
        mCachedWorldDirection = copy.mCachedWorldDirection;
        mBombObjectRecordIndex = copy.mBombObjectRecordIndex;
    }

    void ParticleBomb::beginOperate(osgParticle::Program* program)
    {
        bool absolute = (program->getReferenceFrame() == osgParticle::ParticleProcessor::ABSOLUTE_RF);

        mCachedWorldPosition = absolute ? program->transformLocalToWorld(mPosition) : mPosition;

        // We don't need the direction for Spherical bomb
        if (mSymmetryType != Nif::SymmetryType::Spherical)
        {
            mCachedWorldDirection = absolute ? program->rotateLocalToWorld(mDirection) : mDirection;
            mCachedWorldDirection.normalize();
        }
        if (mBombObjectRecordIndex >= 0 && program->getNumParents() != 0)
        {
            FindGroupByRecordIndex visitor(static_cast<unsigned int>(mBombObjectRecordIndex));
            program->getParent(0)->accept(visitor);
            if (visitor.mFound)
            {
                const osg::Matrix objectToWorld = osg::computeLocalToWorld(visitor.mFoundPath);
                const osg::Vec3f worldPosition = objectToWorld.getTrans();
                mCachedWorldPosition
                    = absolute ? worldPosition : program->transformWorldToLocal(worldPosition);
                if (mSymmetryType != Nif::SymmetryType::Spherical)
                {
                    const osg::Vec3f worldDirection = osg::Matrixf::transform3x3(mDirection, objectToWorld);
                    mCachedWorldDirection
                        = absolute ? worldDirection : program->rotateWorldToLocal(worldDirection);
                    mCachedWorldDirection.normalize();
                }
            }
        }
    }

    void ParticleBomb::operate(osgParticle::Particle* particle, double dt)
    {
        float decay = 1.f;
        osg::Vec3f explosionDir;

        osg::Vec3f particleDir = particle->getPosition() - mCachedWorldPosition;
        float distance = particleDir.length();
        particleDir.normalize();

        switch (mDecayType)
        {
            case Nif::DecayType::None:
                break;
            case Nif::DecayType::Linear:
                decay = 1.f - distance / mRange;
                break;
            case Nif::DecayType::Exponential:
                decay = std::exp(-distance / mRange);
                break;
        }

        if (decay <= 0.f)
            return;

        switch (mSymmetryType)
        {
            case Nif::SymmetryType::Spherical:
                explosionDir = particleDir;
                break;
            case Nif::SymmetryType::Cylindrical:
                explosionDir = particleDir - mCachedWorldDirection * (mCachedWorldDirection * particleDir);
                explosionDir.normalize();
                break;
            case Nif::SymmetryType::Planar:
                explosionDir = mCachedWorldDirection;
                if (explosionDir * particleDir < 0)
                    explosionDir = -explosionDir;
                break;
        }

        particle->addVelocity(explosionDir * mStrength * decay * static_cast<float>(dt));
    }

    ParticleDrag::ParticleDrag(const Nif::NiPSysDragModifier* drag)
        : mPercentage(std::clamp(drag->mPercentage, 0.f, 1.f))
        , mRange(std::max(0.f, drag->mRange))
        , mRangeFalloff(std::max(0.f, drag->mRangeFalloff))
        , mAxis(drag->mDragAxis)
        , mDragObjectRecordIndex(drag->mDragObject.empty() ? -1 : drag->mDragObject->mRecordIndex)
    {
        if (!drag->mDragObject.empty())
            mPosition = drag->mDragObject->mTransform.mTranslation;
        if (mAxis.normalize() == 0.f)
            mAxis.set(0.f, 0.f, 1.f);
    }

    ParticleDrag::ParticleDrag(const ParticleDrag& copy, const osg::CopyOp& copyop)
        : osgParticle::Operator(copy, copyop)
        , mPercentage(copy.mPercentage)
        , mRange(copy.mRange)
        , mRangeFalloff(copy.mRangeFalloff)
        , mPosition(copy.mPosition)
        , mAxis(copy.mAxis)
        , mCachedPosition(copy.mCachedPosition)
        , mCachedAxis(copy.mCachedAxis)
        , mDragObjectRecordIndex(copy.mDragObjectRecordIndex)
    {
    }

    void ParticleDrag::beginOperate(osgParticle::Program* program)
    {
        const bool absolute = program->getReferenceFrame() == osgParticle::ParticleProcessor::ABSOLUTE_RF;
        mCachedPosition = absolute ? program->transformLocalToWorld(mPosition) : mPosition;
        mCachedAxis = absolute ? program->rotateLocalToWorld(mAxis) : mAxis;
        if (mCachedAxis.normalize() == 0.f)
            mCachedAxis.set(0.f, 0.f, 1.f);
        if (mDragObjectRecordIndex >= 0 && program->getNumParents() != 0)
        {
            FindGroupByRecordIndex visitor(static_cast<unsigned int>(mDragObjectRecordIndex));
            program->getParent(0)->accept(visitor);
            if (visitor.mFound)
            {
                const osg::Matrix objectToWorld = osg::computeLocalToWorld(visitor.mFoundPath);
                const osg::Vec3f worldPosition = objectToWorld.getTrans();
                mCachedPosition
                    = absolute ? worldPosition : program->transformWorldToLocal(worldPosition);
                const osg::Vec3f worldAxis = osg::Matrixf::transform3x3(mAxis, objectToWorld);
                mCachedAxis = absolute ? worldAxis : program->rotateWorldToLocal(worldAxis);
                mCachedAxis.normalize();
            }
        }
    }

    void ParticleDrag::operate(osgParticle::Particle* particle, double dt)
    {
        const osg::Vec3f delta = particle->getPosition() - mCachedPosition;
        const float distance = (delta - mCachedAxis * (delta * mCachedAxis)).length();
        float influence = 1.f;
        if (distance > mRange)
        {
            if (mRangeFalloff <= 0.f || distance >= mRange + mRangeFalloff)
                return;
            influence = 1.f - (distance - mRange) / mRangeFalloff;
        }
        const float retention = std::pow(std::max(0.f, 1.f - mPercentage * influence), static_cast<float>(dt));
        particle->setVelocity(particle->getVelocity() * retention);
    }

    ParticleSpawn::ParticleSpawn(const Nif::NiPSysSpawnModifier* spawn)
        : mNumGenerations(spawn->mNumSpawnGenerations)
        , mPercentageSpawned(std::clamp(spawn->mPercentageSpawned, 0.f, 1.f))
        , mMinToSpawn(spawn->mMinNumToSpawn)
        , mMaxToSpawn(std::max(spawn->mMinNumToSpawn, spawn->mMaxNumToSpawn))
        , mSpeedVariation(spawn->mSpawnSpeedVariation)
        , mDirectionVariation(spawn->mSpawnDirVariation)
        , mLifespan(spawn->mLifespan)
        , mLifespanVariation(spawn->mLifespanVariation)
    {
    }

    ParticleSpawn::ParticleSpawn(const ParticleSpawn& copy, const osg::CopyOp& copyop)
        : osgParticle::Operator(copy, copyop)
        , mNumGenerations(copy.mNumGenerations)
        , mPercentageSpawned(copy.mPercentageSpawned)
        , mMinToSpawn(copy.mMinToSpawn)
        , mMaxToSpawn(copy.mMaxToSpawn)
        , mSpeedVariation(copy.mSpeedVariation)
        , mDirectionVariation(copy.mDirectionVariation)
        , mLifespan(copy.mLifespan)
        , mLifespanVariation(copy.mLifespanVariation)
        , mGenerations(copy.mGenerations)
        , mLastAges(copy.mLastAges)
        , mSpawned(copy.mSpawned)
    {
    }

    void ParticleSpawn::operateParticles(osgParticle::ParticleSystem* particleSystem, double dt)
    {
        const int count = particleSystem->numParticles();
        mGenerations.resize(count);
        mLastAges.resize(count);
        mSpawned.resize(count);
        for (int i = 0; i < count; ++i)
        {
            osgParticle::Particle* parent = particleSystem->getParticle(i);
            if (!parent->isAlive())
                continue;
            if (parent->getAge() < mLastAges[i])
            {
                mGenerations[i] = 0;
                mSpawned[i] = false;
            }
            mLastAges[i] = parent->getAge();
            if (mSpawned[i] || mGenerations[i] >= mNumGenerations || parent->getLifeTime() <= 0.0
                || parent->getAge() + dt < parent->getLifeTime())
                continue;
            mSpawned[i] = true;
            if (Misc::Rng::rollClosedProbability() > mPercentageSpawned)
                continue;

            const unsigned int amount = mMinToSpawn == mMaxToSpawn
                ? mMinToSpawn
                : mMinToSpawn + Misc::Rng::rollDice(static_cast<unsigned int>(mMaxToSpawn - mMinToSpawn + 1));
            for (unsigned int childIndex = 0; childIndex < amount; ++childIndex)
            {
                ParticleAgeSetter childTemplate(0.f);
                childTemplate.setShape(parent->getShape());
                childTemplate.setPosition(parent->getPosition());
                osg::Vec3f velocity = parent->getVelocity();
                const float speedScale
                    = std::max(0.f, 1.f + mSpeedVariation * (2.f * Misc::Rng::rollClosedProbability() - 1.f));
                velocity *= speedScale;
                if (mDirectionVariation != 0.f)
                {
                    osg::Vec3f axis(Misc::Rng::rollClosedProbability() * 2.f - 1.f,
                        Misc::Rng::rollClosedProbability() * 2.f - 1.f,
                        Misc::Rng::rollClosedProbability() * 2.f - 1.f);
                    if (axis.normalize() != 0.f)
                        velocity = osg::Quat(mDirectionVariation
                                                * (2.f * Misc::Rng::rollClosedProbability() - 1.f),
                                       axis)
                            * velocity;
                }
                childTemplate.setVelocity(velocity);
                childTemplate.setSizeRange(parent->getSizeRange());
                childTemplate.setColorRange(parent->getColorRange());
                childTemplate.setAlphaRange(parent->getAlphaRange());
                childTemplate.setRadius(parent->getRadius());
                childTemplate.setAngle(parent->getAngle());
                childTemplate.setAngularVelocity(parent->getAngularVelocity());
                childTemplate.setLifeTime(std::max(std::numeric_limits<float>::epsilon(),
                    mLifespan + mLifespanVariation * Misc::Rng::rollClosedProbability()));
                if (osgParticle::Particle* child = particleSystem->createParticle(&childTemplate))
                {
                    const int newCount = particleSystem->numParticles();
                    mGenerations.resize(newCount);
                    mLastAges.resize(newCount);
                    mSpawned.resize(newCount);
                    for (int candidate = 0; candidate < newCount; ++candidate)
                        if (particleSystem->getParticle(candidate) == child)
                        {
                            mGenerations[candidate] = static_cast<unsigned short>(mGenerations[i] + 1);
                            mLastAges[candidate] = 0.0;
                            mSpawned[candidate] = false;
                            break;
                        }
                }
            }
        }
    }

    Emitter::Emitter()
        : osgParticle::Emitter()
        , mFlags(0)
        , mGeometryEmitterTarget(std::nullopt)
    {
    }

    Emitter::Emitter(const Emitter& copy, const osg::CopyOp& copyop)
        : osgParticle::Emitter(copy, copyop)
        , mTargets(copy.mTargets)
        , mPlacer(copy.mPlacer)
        , mShooter(copy.mShooter)
        // need a deep copy because the remainder is stored in the object
        , mCounter(static_cast<osgParticle::Counter*>(copy.mCounter->clone(osg::CopyOp::DEEP_COPY_ALL)))
        , mFlags(copy.mFlags)
        , mGeometryEmitterTarget(copy.mGeometryEmitterTarget)
        , mCachedGeometryEmitter(copy.mCachedGeometryEmitter)
    {
    }

    Emitter::Emitter(const std::vector<int>& targets)
        : mTargets(targets)
        , mFlags(0)
        , mGeometryEmitterTarget(std::nullopt)
    {
    }

    void Emitter::emitParticles(double dt)
    {
        int n = mCounter->numParticlesToCreate(dt);
        if (n == 0)
            return;

        osg::Matrix worldToPs;

        // maybe this could be optimized by halting at the lowest common ancestor of the particle and emitter nodes
        osg::NodePathList partsysNodePaths = getParticleSystem()->getParentalNodePaths();
        if (!partsysNodePaths.empty())
        {
            osg::Matrix psToWorld = osg::computeLocalToWorld(partsysNodePaths[0]);
            worldToPs = osg::Matrix::inverse(psToWorld);
        }

        const osg::Matrix& ltw = getLocalToWorldMatrix();
        osg::Matrix emitterToPs = ltw * worldToPs;

        osg::ref_ptr<osg::Vec3Array> geometryVertices = nullptr;

        const bool useGeometryEmitter = mFlags & Nif::NiParticleSystemController::BSPArrayController_AtVertex;

        if (useGeometryEmitter || !mTargets.empty())
        {
            int recordIndex;

            if (useGeometryEmitter && mTargets.empty())
            {
                if (!mGeometryEmitterTarget.has_value())
                    return;

                recordIndex = mGeometryEmitterTarget.value();
            }
            else
            {
                size_t randomIndex = Misc::Rng::rollDice(mTargets.size());
                recordIndex = mTargets[randomIndex];
            }

            // we could use a map here for faster lookup
            FindGroupByRecordIndex visitor(recordIndex);
            getParent(0)->accept(visitor);

            if (!visitor.mFound)
            {
                Log(Debug::Info) << "Can't find emitter node" << recordIndex;
                return;
            }

            if (useGeometryEmitter)
            {
                if (!mCachedGeometryEmitter.lock(geometryVertices))
                {
                    FindFirstGeometry geometryVisitor;
                    visitor.mFound->accept(geometryVisitor);

                    if (geometryVisitor.mGeometry)
                    {
                        if (auto* vertices = dynamic_cast<osg::Vec3Array*>(geometryVisitor.mGeometry->getVertexArray()))
                        {
                            mCachedGeometryEmitter = osg::observer_ptr<osg::Vec3Array>(vertices);
                            geometryVertices = vertices;
                        }
                    }
                }
            }

            osg::NodePath path = visitor.mFoundPath;
            path.erase(path.begin());
            if (!useGeometryEmitter && (mFlags & Nif::NiParticleSystemController::BSPArrayController_AtNode)
                && path.size())
            {
                osg::Matrix current;

                LocalToWorldAccumulator accum(current);
                accum.accumulate(path);

                osg::Matrix parent = accum.mLastMatrix.value_or(current);

                auto p1 = parent.getTrans();
                auto p2 = current.getTrans();
                current.setTrans((p2 - p1) * Misc::Rng::rollClosedProbability() + p1);

                emitterToPs = current * emitterToPs;
            }
            else
            {
                emitterToPs = osg::computeLocalToWorld(path) * emitterToPs;
            }
        }

        emitterToPs.orthoNormalize(emitterToPs);

        if (useGeometryEmitter && (!geometryVertices.valid() || geometryVertices->empty()))
            return;

        for (int i = 0; i < n; ++i)
        {
            osgParticle::Particle* const particle = getParticleSystem()->createParticle(nullptr);
            if (particle)
            {
                if (useGeometryEmitter)
                    particle->setPosition((*geometryVertices)[Misc::Rng::rollDice(geometryVertices->getNumElements())]);
                else if (mPlacer)
                    mPlacer->place(particle);

                mShooter->shoot(particle);

                particle->transformPositionVelocity(emitterToPs);
            }
        }
    }

    ModernParticleController::ModernParticleController(Inputs inputs, ParticleShooter* shooter,
        GravityAffector* gravity, float speedVariation, float declination, float declinationVariation,
        float planarAngle, float planarAngleVariation, float radiusVariation, float lifetimeVariation)
        : mInputs(std::move(inputs))
        , mShooter(shooter)
        , mGravity(gravity)
        , mSpeedVariation(speedVariation)
        , mDeclination(declination)
        , mDeclinationVariation(declinationVariation)
        , mPlanarAngle(planarAngle)
        , mPlanarAngleVariation(planarAngleVariation)
        , mRadiusVariation(radiusVariation)
        , mLifetimeVariation(lifetimeVariation)
    {
    }

    ModernParticleController::ModernParticleController(
        const ModernParticleController& copy, const osg::CopyOp& copyop)
        : SceneUtil::NodeCallback<ModernParticleController, osgParticle::Emitter*>(copy, copyop)
        , SceneUtil::Controller(copy)
        , mInputs(copy.mInputs)
        , mShooter(copy.mShooter)
        , mGravity(copy.mGravity)
        , mSpeedVariation(copy.mSpeedVariation)
        , mDeclination(copy.mDeclination)
        , mDeclinationVariation(copy.mDeclinationVariation)
        , mPlanarAngle(copy.mPlanarAngle)
        , mPlanarAngleVariation(copy.mPlanarAngleVariation)
        , mRadiusVariation(copy.mRadiusVariation)
        , mLifetimeVariation(copy.mLifetimeVariation)
    {
    }

    void ModernParticleController::operator()(osgParticle::Emitter* emitter, osg::NodeVisitor* nv)
    {
        if (hasInput())
        {
            const float time = getInputValue(nv);
            bool active = mInputs.mActive.empty() || mInputs.mActive.interpKey(time);
            emitter->setEnabled(active);
            auto* nifEmitter = dynamic_cast<Emitter*>(emitter);
            if (auto* counter = dynamic_cast<osgParticle::ConstantRateCounter*>(
                    nifEmitter != nullptr ? nifEmitter->getCounter() : nullptr))
                if (!mInputs.mBirthRate.empty())
                    counter->setNumberOfParticlesPerSecondToCreate(std::max(0.f, mInputs.mBirthRate.interpKey(time)));
            if (mShooter)
            {
                if (!mInputs.mSpeed.empty())
                    mShooter->setSpeed(mInputs.mSpeed.interpKey(time), mSpeedVariation);
                const float declination
                    = mInputs.mDeclination.empty() ? mDeclination : mInputs.mDeclination.interpKey(time);
                const float variation = mInputs.mDeclinationVariation.empty()
                    ? mDeclinationVariation
                    : mInputs.mDeclinationVariation.interpKey(time);
                mShooter->setAngles(declination, variation, mPlanarAngle, mPlanarAngleVariation);
                if (!mInputs.mLifetime.empty())
                    mShooter->setLifetime(mInputs.mLifetime.interpKey(time), mLifetimeVariation);
            }
            if (mGravity && !mInputs.mGravityStrength.empty())
                mGravity->setForce(mInputs.mGravityStrength.interpKey(time));
            if (!mInputs.mInitialRadius.empty())
            {
                const float radius = std::max(0.f, mInputs.mInitialRadius.interpKey(time));
                mShooter->setRadius(radius, mRadiusVariation);
                emitter->getParticleSystem()->getDefaultParticleTemplate().setSizeRange(
                    osgParticle::rangef(radius, radius));
            }
        }
        traverse(emitter, nv);
    }

    FindGroupByRecordIndex::FindGroupByRecordIndex(unsigned int recordIndex)
        : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
        , mFound(nullptr)
        , mRecordIndex(recordIndex)
    {
    }

    void FindGroupByRecordIndex::apply(osg::Node& node)
    {
        applyNode(node);
    }

    void FindGroupByRecordIndex::apply(osg::MatrixTransform& node)
    {
        applyNode(node);
    }

    void FindGroupByRecordIndex::apply(osg::Geometry& node)
    {
        applyNode(node);
    }

    void FindGroupByRecordIndex::applyNode(osg::Node& searchNode)
    {
        unsigned int recordIndex;
        if (searchNode.getUserValue("recordIndex", recordIndex) && mRecordIndex == recordIndex)
        {
            osg::Group* group = searchNode.asGroup();
            if (!group)
                group = searchNode.getParent(0);

            mFound = group;
            mFoundPath = getNodePath();
            return;
        }
        traverse(searchNode);
    }

    PlanarCollider::PlanarCollider(const Nif::NiPlanarCollider* collider)
        : mBounceFactor(collider->mBounceFactor)
        , mExtents(collider->mExtents)
        , mPosition(collider->mPosition)
        , mXVector(collider->mXVector)
        , mYVector(collider->mYVector)
        , mPlane(-collider->mPlaneNormal, collider->mPlaneDistance)
    {
    }

    PlanarCollider::PlanarCollider(const Nif::NiPSysPlanarCollider* collider)
        : mBounceFactor(collider->mBounce)
        // The legacy implementation stores the two extents in the opposite
        // order to its basis vectors.
        , mExtents(collider->mHeight, collider->mWidth)
        , mXVector(collider->mXAxis)
        , mYVector(collider->mYAxis)
        , mColliderObjectRecordIndex(
              collider->mColliderObject.empty() ? -1 : collider->mColliderObject->mRecordIndex)
    {
        if (!collider->mColliderObject.empty())
            mPosition = collider->mColliderObject->mTransform.mTranslation;
        mXVector.normalize();
        mYVector.normalize();
        osg::Vec3f normal = mXVector ^ mYVector;
        normal.normalize();
        mPlane.set(normal, mPosition);
    }

    PlanarCollider::PlanarCollider(const PlanarCollider& copy, const osg::CopyOp& copyop)
        : osgParticle::Operator(copy, copyop)
        , mBounceFactor(copy.mBounceFactor)
        , mExtents(copy.mExtents)
        , mPosition(copy.mPosition)
        , mPositionInParticleSpace(copy.mPositionInParticleSpace)
        , mXVector(copy.mXVector)
        , mXVectorInParticleSpace(copy.mXVectorInParticleSpace)
        , mYVector(copy.mYVector)
        , mYVectorInParticleSpace(copy.mYVectorInParticleSpace)
        , mPlane(copy.mPlane)
        , mPlaneInParticleSpace(copy.mPlaneInParticleSpace)
        , mColliderObjectRecordIndex(copy.mColliderObjectRecordIndex)
    {
    }

    void PlanarCollider::beginOperate(osgParticle::Program* program)
    {
        mPositionInParticleSpace = mPosition;
        mPlaneInParticleSpace = mPlane;
        mXVectorInParticleSpace = mXVector;
        mYVectorInParticleSpace = mYVector;
        if (program->getReferenceFrame() == osgParticle::ParticleProcessor::ABSOLUTE_RF)
        {
            mPositionInParticleSpace = program->transformLocalToWorld(mPosition);
            mPlaneInParticleSpace.transform(program->getLocalToWorldMatrix());
            mXVectorInParticleSpace = program->rotateLocalToWorld(mXVector);
            mYVectorInParticleSpace = program->rotateLocalToWorld(mYVector);
        }
        if (mColliderObjectRecordIndex >= 0 && program->getNumParents() != 0)
        {
            FindGroupByRecordIndex visitor(static_cast<unsigned int>(mColliderObjectRecordIndex));
            program->getParent(0)->accept(visitor);
            if (visitor.mFound)
            {
                const osg::Matrix objectToWorld = osg::computeLocalToWorld(visitor.mFoundPath);
                const bool absolute
                    = program->getReferenceFrame() == osgParticle::ParticleProcessor::ABSOLUTE_RF;
                const osg::Vec3f worldPosition = objectToWorld.getTrans();
                mPositionInParticleSpace
                    = absolute ? worldPosition : program->transformWorldToLocal(worldPosition);
                const osg::Vec3f worldX = osg::Matrixf::transform3x3(mXVector, objectToWorld);
                const osg::Vec3f worldY = osg::Matrixf::transform3x3(mYVector, objectToWorld);
                mXVectorInParticleSpace = absolute ? worldX : program->rotateWorldToLocal(worldX);
                mYVectorInParticleSpace = absolute ? worldY : program->rotateWorldToLocal(worldY);
                osg::Vec3f normal = mXVectorInParticleSpace ^ mYVectorInParticleSpace;
                normal.normalize();
                mPlaneInParticleSpace.set(normal, mPositionInParticleSpace);
            }
        }
    }

    void PlanarCollider::operate(osgParticle::Particle* particle, double dt)
    {
        const osg::Vec3 normal = mPlaneInParticleSpace.getNormal();

        // Does the particle in question move towards the collider?
        float distToPlane = mPlaneInParticleSpace.distance(particle->getPosition());
        float velDotProduct = particle->getVelocity() * normal;
        if (distToPlane * velDotProduct >= 0.0f)
            return;

        // Would it cross the collider?
        float nextDistToPlane = distToPlane + velDotProduct * static_cast<float>(dt);
        if (distToPlane * nextDistToPlane > 0.0f)
            return;

        // Is it inside the collider's bounds?
        osg::Vec3f relativePos = particle->getPosition() - mPositionInParticleSpace;
        float xDotProduct = relativePos * mXVectorInParticleSpace;
        float yDotProduct = relativePos * mYVectorInParticleSpace;
        // NB: extent components are intentionally swapped
        if (-mExtents.y() * 0.5f > xDotProduct || mExtents.y() * 0.5f < xDotProduct)
            return;
        if (-mExtents.x() * 0.5f > yDotProduct || mExtents.x() * 0.5f < yDotProduct)
            return;

        // Deflect the particle
        osg::Vec3 reflectedVelocity = particle->getVelocity() - normal * (2 * velDotProduct);
        reflectedVelocity *= mBounceFactor;
        particle->setVelocity(reflectedVelocity);
    }

    SphericalCollider::SphericalCollider(const Nif::NiSphericalCollider* collider)
        : mBounceFactor(collider->mBounceFactor)
        , mSphere(collider->mCenter, collider->mRadius)
    {
    }

    SphericalCollider::SphericalCollider(const Nif::NiPSysSphericalCollider* collider)
        : mBounceFactor(collider->mBounce)
        , mSphere(collider->mColliderObject.empty() ? osg::Vec3f() : collider->mColliderObject->mTransform.mTranslation,
              collider->mRadius)
        , mColliderObjectRecordIndex(
              collider->mColliderObject.empty() ? -1 : collider->mColliderObject->mRecordIndex)
    {
    }

    SphericalCollider::SphericalCollider()
        : mBounceFactor(1.0f)
    {
    }

    SphericalCollider::SphericalCollider(const SphericalCollider& copy, const osg::CopyOp& copyop)
        : osgParticle::Operator(copy, copyop)
        , mBounceFactor(copy.mBounceFactor)
        , mSphere(copy.mSphere)
        , mSphereInParticleSpace(copy.mSphereInParticleSpace)
        , mColliderObjectRecordIndex(copy.mColliderObjectRecordIndex)
    {
    }

    void SphericalCollider::beginOperate(osgParticle::Program* program)
    {
        mSphereInParticleSpace = mSphere;
        if (program->getReferenceFrame() == osgParticle::ParticleProcessor::ABSOLUTE_RF)
            mSphereInParticleSpace.center() = program->transformLocalToWorld(mSphereInParticleSpace.center());
        if (mColliderObjectRecordIndex >= 0 && program->getNumParents() != 0)
        {
            FindGroupByRecordIndex visitor(static_cast<unsigned int>(mColliderObjectRecordIndex));
            program->getParent(0)->accept(visitor);
            if (visitor.mFound)
            {
                const osg::Vec3f worldPosition = osg::computeLocalToWorld(visitor.mFoundPath).getTrans();
                mSphereInParticleSpace.center()
                    = program->getReferenceFrame() == osgParticle::ParticleProcessor::ABSOLUTE_RF
                    ? worldPosition
                    : program->transformWorldToLocal(worldPosition);
            }
        }
    }

    void SphericalCollider::operate(osgParticle::Particle* particle, double dt)
    {
        if (particle->getVelocity().length2() <= std::numeric_limits<float>::epsilon())
            return;
        osg::Vec3f cent
            = (particle->getPosition() - mSphereInParticleSpace.center()); // vector from sphere center to particle

        bool insideSphere = cent.length2() <= mSphereInParticleSpace.radius2();

        if (insideSphere
            || (cent * particle->getVelocity()
                < 0.0f)) // if outside, make sure the particle is flying towards the sphere
        {
            // Collision test (finding point of contact) is performed by solving a quadratic equation:
            // ||vec(cent) + vec(vel)*k|| = R      /^2
            // k^2 + 2*k*(vec(cent)*vec(vel))/||vec(vel)||^2 + (||vec(cent)||^2 - R^2)/||vec(vel)||^2 = 0

            float b = -(cent * particle->getVelocity()) / particle->getVelocity().length2();

            osg::Vec3f u = cent + particle->getVelocity() * b;

            if (insideSphere || (u.length2() < mSphereInParticleSpace.radius2()))
            {
                float d = (mSphereInParticleSpace.radius2() - u.length2()) / particle->getVelocity().length2();
                float k = insideSphere ? (std::sqrt(d) + b) : (b - std::sqrt(d));

                if (k < dt)
                {
                    // collision detected; reflect off the tangent plane
                    osg::Vec3f contact = particle->getPosition() + particle->getVelocity() * k;

                    osg::Vec3 normal = (contact - mSphereInParticleSpace.center());
                    normal.normalize();

                    float dotproduct = particle->getVelocity() * normal;

                    osg::Vec3 reflectedVelocity = particle->getVelocity() - normal * (2 * dotproduct);
                    reflectedVelocity *= mBounceFactor;
                    particle->setVelocity(reflectedVelocity);
                }
            }
        }
    }

}
