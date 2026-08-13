#ifndef OPENMW_COMPONENTS_NIFOSG_PARTICLE_H
#define OPENMW_COMPONENTS_NIFOSG_PARTICLE_H

#include <optional>

#include <osgParticle/Counter>
#include <osgParticle/Emitter>
#include <osgParticle/Operator>
#include <osgParticle/Particle>
#include <osgParticle/Placer>
#include <osgParticle/Shooter>


#include <components/nif/particle.hpp> // NiGravity::ForceType

#include <components/sceneutil/nodecallback.hpp>

#include "controller.hpp" // ValueInterpolator

namespace Nif
{
    struct NiColorData;
    struct NiPSysBombModifier;
    struct NiPSysDragModifier;
    struct NiPSysPlanarCollider;
    struct NiPSysRotationModifier;
    struct NiPSysSpawnModifier;
    struct NiPSysSphericalCollider;
}

namespace NifOsg
{

    class VolumePlacer : public osgParticle::Placer
    {
    public:
        enum class Shape
        {
            Box,
            Cylinder,
            Sphere,
        };

        VolumePlacer() = default;
        VolumePlacer(Shape shape, const osg::Vec3f& extents);
        VolumePlacer(const VolumePlacer& copy, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);

        META_Object(NifOsg, VolumePlacer)

        void place(osgParticle::Particle* particle) const override;
        osg::Vec3 getControlPosition() const override { return osg::Vec3(); }

    private:
        Shape mShape{ Shape::Box };
        osg::Vec3f mExtents;
    };

    // Subclass ParticleSystem to support a limit on the number of active particles.
    class ParticleSystem : public osgParticle::ParticleSystem
    {
    public:
        ParticleSystem();
        ParticleSystem(const ParticleSystem& copy, const osg::CopyOp& copyop);

        META_Object(NifOsg, ParticleSystem)

        osgParticle::Particle* createParticle(const osgParticle::Particle* ptemplate) override;

        void setQuota(int quota);

        void drawImplementation(osg::RenderInfo& renderInfo) const override;

    private:
        int mQuota;
        osg::ref_ptr<osg::Vec3Array> mNormalArray;
    };

    // HACK: Particle doesn't allow setting the initial age, but we need this for loading the particle system state
    class ParticleAgeSetter : public osgParticle::Particle
    {
    public:
        ParticleAgeSetter(float age)
            : Particle()
        {
            _t0 = age;
        }
    };

    // Node callback used to set the inverse of the parent's world matrix on the MatrixTransform
    // that the callback is attached to. Used for certain particle systems,
    // so that the particles do not move with the node they are attached to.
    class InverseWorldMatrix : public SceneUtil::NodeCallback<InverseWorldMatrix, osg::MatrixTransform*>
    {
    public:
        InverseWorldMatrix() {}
        InverseWorldMatrix(const InverseWorldMatrix& copy, const osg::CopyOp& copyop)
            : osg::Object(copy, copyop)
            , SceneUtil::NodeCallback<InverseWorldMatrix, osg::MatrixTransform*>(copy, copyop)
        {
        }

        META_Object(NifOsg, InverseWorldMatrix)

        void operator()(osg::MatrixTransform* node, osg::NodeVisitor* nv);
    };

    class ParticleShooter : public osgParticle::Shooter
    {
    public:
        ParticleShooter(float minSpeed, float maxSpeed, float horizontalDir, float horizontalAngle, float verticalDir,
            float verticalAngle, float lifetime, float lifetimeRandom);
        ParticleShooter();
        ParticleShooter(const ParticleShooter& copy, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);

        ParticleShooter& operator=(const ParticleShooter&) = delete;

        META_Object(NifOsg, ParticleShooter)

        void shoot(osgParticle::Particle* particle) const override;

        void setSpeed(float speed, float variation);
        void setAngles(float declination, float declinationVariation, float planarAngle, float planarAngleVariation);
        void setLifetime(float lifetime, float variation);
        void setRadius(float radius, float variation);
        void setRotation(const Nif::NiPSysRotationModifier* rotation);

    private:
        float mMinSpeed;
        float mMaxSpeed;
        float mHorizontalDir;
        float mHorizontalAngle;
        float mVerticalDir;
        float mVerticalAngle;
        float mLifetime;
        float mLifetimeRandom;
        float mRadius{ 1.f };
        float mRadiusVariation{ 0.f };
        float mRotationSpeed{ 0.f };
        float mRotationSpeedVariation{ 0.f };
        float mRotationAngle{ 0.f };
        float mRotationAngleVariation{ 0.f };
        bool mRandomRotationSpeedSign{ false };
        bool mRandomRotationAxis{ false };
        osg::Vec3f mRotationAxis{ 0.f, 0.f, 1.f };
    };

    class PlanarCollider : public osgParticle::Operator
    {
    public:
        PlanarCollider(const Nif::NiPlanarCollider* collider);
        PlanarCollider(const Nif::NiPSysPlanarCollider* collider);
        PlanarCollider() = default;
        PlanarCollider(const PlanarCollider& copy, const osg::CopyOp& copyop);

        META_Object(NifOsg, PlanarCollider)

        void beginOperate(osgParticle::Program* program) override;
        void operate(osgParticle::Particle* particle, double dt) override;

    private:
        float mBounceFactor{ 0.f };
        osg::Vec2f mExtents;
        osg::Vec3f mPosition, mPositionInParticleSpace;
        osg::Vec3f mXVector, mXVectorInParticleSpace;
        osg::Vec3f mYVector, mYVectorInParticleSpace;
        osg::Plane mPlane, mPlaneInParticleSpace;
        int mColliderObjectRecordIndex{ -1 };
    };

    class SphericalCollider : public osgParticle::Operator
    {
    public:
        SphericalCollider(const Nif::NiSphericalCollider* collider);
        SphericalCollider(const Nif::NiPSysSphericalCollider* collider);
        SphericalCollider();
        SphericalCollider(const SphericalCollider& copy, const osg::CopyOp& copyop);

        META_Object(NifOsg, SphericalCollider)

        void beginOperate(osgParticle::Program* program) override;
        void operate(osgParticle::Particle* particle, double dt) override;

    private:
        float mBounceFactor;
        osg::BoundingSphere mSphere;
        osg::BoundingSphere mSphereInParticleSpace;
        int mColliderObjectRecordIndex{ -1 };
    };

    class GrowFadeAffector : public osgParticle::Operator
    {
    public:
        GrowFadeAffector(float growTime, float fadeTime);
        GrowFadeAffector();
        GrowFadeAffector(const GrowFadeAffector& copy, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);

        GrowFadeAffector& operator=(const GrowFadeAffector&) = delete;

        META_Object(NifOsg, GrowFadeAffector)

        void beginOperate(osgParticle::Program* program) override;
        void operate(osgParticle::Particle* particle, double dt) override;

    private:
        float mGrowTime;
        float mFadeTime;

        float mCachedDefaultSize;
    };

    class ParticleColorAffector : public osgParticle::Operator
    {
    public:
        ParticleColorAffector(const Nif::NiColorData* clrdata);
        ParticleColorAffector();
        ParticleColorAffector(const ParticleColorAffector& copy, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);

        ParticleColorAffector& operator=(const ParticleColorAffector&) = delete;

        META_Object(NifOsg, ParticleColorAffector)

        void operate(osgParticle::Particle* particle, double dt) override;

    private:
        Vec4Interpolator mData;
    };

    class GravityAffector : public osgParticle::Operator
    {
    public:
        GravityAffector(const Nif::NiGravity* gravity);
        GravityAffector() = default;
        GravityAffector(const GravityAffector& copy, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);

        GravityAffector& operator=(const GravityAffector&) = delete;

        META_Object(NifOsg, GravityAffector)

        void operate(osgParticle::Particle* particle, double dt) override;
        void beginOperate(osgParticle::Program*) override;

        void setForce(float force) { mForce = force; }

    private:
        float mForce{ 0.f };
        Nif::ForceType mType{ Nif::ForceType::Wind };
        osg::Vec3f mPosition;
        osg::Vec3f mDirection;
        float mDecay{ 0.f };
        osg::Vec3f mCachedWorldPosition;
        osg::Vec3f mCachedWorldDirection;
    };

    class ParticleBomb : public osgParticle::Operator
    {
    public:
        ParticleBomb(const Nif::NiParticleBomb* bomb);
        ParticleBomb(const Nif::NiPSysBombModifier* bomb);
        ParticleBomb() = default;
        ParticleBomb(const ParticleBomb& copy, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);

        ParticleBomb& operator=(const ParticleBomb&) = delete;

        META_Object(NifOsg, ParticleBomb)

        void operate(osgParticle::Particle* particle, double dt) override;
        void beginOperate(osgParticle::Program*) override;

    private:
        float mRange{ 0.f };
        float mStrength{ 0.f };
        Nif::DecayType mDecayType{ Nif::DecayType::None };
        Nif::SymmetryType mSymmetryType{ Nif::SymmetryType::Spherical };
        osg::Vec3f mPosition;
        osg::Vec3f mDirection;
        osg::Vec3f mCachedWorldPosition;
        osg::Vec3f mCachedWorldDirection;
        int mBombObjectRecordIndex{ -1 };
    };

    class ParticleDrag : public osgParticle::Operator
    {
    public:
        ParticleDrag(const Nif::NiPSysDragModifier* drag);
        ParticleDrag() = default;
        ParticleDrag(const ParticleDrag& copy, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);

        META_Object(NifOsg, ParticleDrag)

        void beginOperate(osgParticle::Program* program) override;
        void operate(osgParticle::Particle* particle, double dt) override;

    private:
        float mPercentage{ 0.f };
        float mRange{ 0.f };
        float mRangeFalloff{ 0.f };
        osg::Vec3f mPosition;
        osg::Vec3f mAxis{ 0.f, 0.f, 1.f };
        osg::Vec3f mCachedPosition;
        osg::Vec3f mCachedAxis{ 0.f, 0.f, 1.f };
        int mDragObjectRecordIndex{ -1 };
    };

    class ParticleSpawn : public osgParticle::Operator
    {
    public:
        ParticleSpawn(const Nif::NiPSysSpawnModifier* spawn);
        ParticleSpawn() = default;
        ParticleSpawn(const ParticleSpawn& copy, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);

        META_Object(NifOsg, ParticleSpawn)

        void operate(osgParticle::Particle*, double) override {}
        void operateParticles(osgParticle::ParticleSystem* particleSystem, double dt) override;

    private:
        unsigned short mNumGenerations{ 0 };
        float mPercentageSpawned{ 0.f };
        unsigned short mMinToSpawn{ 0 };
        unsigned short mMaxToSpawn{ 0 };
        float mSpeedVariation{ 0.f };
        float mDirectionVariation{ 0.f };
        float mLifespan{ 0.f };
        float mLifespanVariation{ 0.f };
        std::vector<unsigned short> mGenerations;
        std::vector<double> mLastAges;
        std::vector<bool> mSpawned;
    };

    // NodeVisitor to find a Group node with the given record index, stored in the node's user data container.
    // Alternatively, returns the node's parent Group if that node is not a Group (i.e. a leaf node).
    class FindGroupByRecordIndex : public osg::NodeVisitor
    {
    public:
        FindGroupByRecordIndex(unsigned int recordIndex);

        void apply(osg::Node& node) override;

        // Technically not required as the default implementation would trickle down to apply(Node&) anyway,
        // but we'll shortcut instead to avoid the chain of virtual function calls
        void apply(osg::MatrixTransform& node) override;
        void apply(osg::Geometry& node) override;

        void applyNode(osg::Node& searchNode);

        osg::Group* mFound;
        osg::NodePath mFoundPath;

    private:
        unsigned int mRecordIndex;
    };

    // Subclass emitter to support randomly choosing one of the child node's transforms for the emit position of new
    // particles.
    class Emitter : public osgParticle::Emitter
    {
    public:
        Emitter(const std::vector<int>& targets);
        Emitter();
        Emitter(const Emitter& copy, const osg::CopyOp& copyop);

        META_Object(NifOsg, Emitter)

        void emitParticles(double dt) override;

        void setShooter(osgParticle::Shooter* shooter) { mShooter = shooter; }
        void setPlacer(osgParticle::Placer* placer) { mPlacer = placer; }
        void setCounter(osgParticle::Counter* counter) { mCounter = counter; }
        osgParticle::Counter* getCounter() { return mCounter; }
        void setGeometryEmitterTarget(std::optional<int> recordIndex) { mGeometryEmitterTarget = recordIndex; }
        void setFlags(int flags) { mFlags = flags; }

    private:
        // NIF Record indices
        std::vector<int> mTargets;

        osg::ref_ptr<osgParticle::Placer> mPlacer;
        osg::ref_ptr<osgParticle::Shooter> mShooter;
        osg::ref_ptr<osgParticle::Counter> mCounter;

        int mFlags;

        std::optional<int> mGeometryEmitterTarget;
        osg::observer_ptr<osg::Vec3Array> mCachedGeometryEmitter;
    };

    class ModernParticleController
        : public SceneUtil::NodeCallback<ModernParticleController, osgParticle::Emitter*>, public SceneUtil::Controller
    {
    public:
        struct Inputs
        {
            FloatInterpolator mBirthRate;
            FloatInterpolator mSpeed;
            FloatInterpolator mDeclination;
            FloatInterpolator mDeclinationVariation;
            FloatInterpolator mInitialRadius;
            FloatInterpolator mLifetime;
            FloatInterpolator mGravityStrength;
            BoolInterpolator mActive;
        };

        ModernParticleController() = default;
        ModernParticleController(Inputs inputs, ParticleShooter* shooter, GravityAffector* gravity,
            float speedVariation, float declination, float declinationVariation, float planarAngle,
            float planarAngleVariation, float radiusVariation, float lifetimeVariation);
        ModernParticleController(const ModernParticleController& copy, const osg::CopyOp& copyop);

        META_Object(NifOsg, ModernParticleController)

        void operator()(osgParticle::Emitter* emitter, osg::NodeVisitor* nv);

    private:
        Inputs mInputs;
        osg::ref_ptr<ParticleShooter> mShooter;
        osg::ref_ptr<GravityAffector> mGravity;
        float mSpeedVariation{ 0.f };
        float mDeclination{ 0.f };
        float mDeclinationVariation{ 0.f };
        float mPlanarAngle{ 0.f };
        float mPlanarAngleVariation{ 0.f };
        float mRadiusVariation{ 0.f };
        float mLifetimeVariation{ 0.f };
    };

}

#endif
