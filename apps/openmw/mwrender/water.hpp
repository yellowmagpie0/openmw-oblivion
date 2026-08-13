#ifndef OPENMW_MWRENDER_WATER_H
#define OPENMW_MWRENDER_WATER_H

#include <memory>
#include <vector>

#include <osg/Vec3d>
#include <osg/Vec3f>
#include <osg/Vec4f>
#include <osg/ref_ptr>

#include <components/settings/settings.hpp>
#include <components/esm/refid.hpp>
#include <components/vfs/pathutil.hpp>

namespace osg
{
    class Group;
    class PositionAttitudeTransform;
    class Geometry;
    class Node;
    class Callback;
}

namespace osgUtil
{
    class IncrementalCompileOperation;
}

namespace Resource
{
    class ResourceSystem;
}

namespace MWWorld
{
    class CellStore;
    class Ptr;
}

namespace Fallback
{
    class Map;
}

namespace MWRender
{

    class Refraction;
    class Reflection;
    class RippleSimulation;
    class RainSettingsUpdater;
    class Ripples;

    /// Water rendering
    class Water
    {
        osg::ref_ptr<RainSettingsUpdater> mRainSettingsUpdater;

        osg::ref_ptr<osg::Group> mParent;
        osg::ref_ptr<osg::Group> mSceneRoot;
        osg::ref_ptr<osg::PositionAttitudeTransform> mWaterNode;
        osg::ref_ptr<osg::Geometry> mWaterGeom;
        osg::ref_ptr<osg::Geometry> mSimpleWaterGeom;
        Resource::ResourceSystem* mResourceSystem;
        osg::ref_ptr<osgUtil::IncrementalCompileOperation> mIncrementalCompileOperation;

        std::unique_ptr<RippleSimulation> mSimulation;

        osg::ref_ptr<Refraction> mRefraction;
        osg::ref_ptr<Reflection> mReflection;
        osg::ref_ptr<Ripples> mRipples;

        bool mEnabled;
        bool mToggled;
        float mTop;
        bool mInterior;
        bool mShowWorld;
        osg::Vec4f mWaterColor;
        osg::Vec4f mWaterDeepColor;
        VFS::Path::Normalized mNativeTexture;
        ESM::RefId mWaterType;
        osg::Vec2f mWindDirection;
        float mWindVelocity;
        float mWaveAmplitude;
        float mWaveFrequency;
        float mReflectivity;
        float mFresnelAmount;

        osg::Callback* mCullCallback;
        osg::ref_ptr<osg::Callback> mShaderWaterStateSetUpdater;

        osg::Vec3f getSceneNodeCoordinates(int gridX, int gridY, ESM::RefId worldspace);
        void updateVisible();

        void createSimpleWaterStateSet(osg::Node* node, float alpha);

        void createShaderWaterStateSet(osg::Node* node);

        void updateWaterMaterial();

    public:
        Water(osg::Group* parent, osg::Group* sceneRoot, Resource::ResourceSystem* resourceSystem,
            osgUtil::IncrementalCompileOperation* ico);
        ~Water();

        void setCullCallback(osg::Callback* callback);

        void listAssetsToPreload(std::vector<VFS::Path::Normalized>& textures);

        void setEnabled(bool enabled);

        bool toggle();

        bool isUnderwater(const osg::Vec3f& pos) const;

        /// adds an emitter, position will be tracked automatically using its scene node
        void addEmitter(const MWWorld::Ptr& ptr, float scale = 1.f, float force = 1.f);
        void removeEmitter(const MWWorld::Ptr& ptr);
        void updateEmitterPtr(const MWWorld::Ptr& old, const MWWorld::Ptr& ptr);
        void emitRipple(const osg::Vec3f& pos);

        void removeCell(const MWWorld::CellStore* store); ///< remove all emitters in this cell

        void clearRipples();

        void changeCell(const MWWorld::CellStore* store);
        void setHeight(const float height);
        void setRainIntensity(const float rainIntensity);

        void update(float dt, bool paused);

        osg::Vec3d getPosition() const;
        const osg::Vec4f& getWaterColor() const { return mWaterColor; }
        const osg::Vec4f& getWaterDeepColor() const { return mWaterDeepColor; }
        const osg::Vec2f& getWindDirection() const { return mWindDirection; }
        float getWindVelocity() const { return mWindVelocity; }
        float getWaveAmplitude() const { return mWaveAmplitude; }
        float getWaveFrequency() const { return mWaveFrequency; }
        float getReflectivity() const { return mReflectivity; }
        float getFresnelAmount() const { return mFresnelAmount; }

        void processChangedSettings(const Settings::CategorySettingVector& settings);

        void showWorld(bool show);
    };

}

#endif
