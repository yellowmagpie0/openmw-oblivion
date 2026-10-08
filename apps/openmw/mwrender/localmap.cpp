#include "localmap.hpp"

#include <cstdint>
#include <limits>

#include <osg/ComputeBoundsVisitor>
#include <osg/Fog>
#include <osg/PolygonMode>
#include <osg/Texture2D>

#include <osgDB/ReadFile>

#include <components/debug/debuglog.hpp>
#include <components/esm3/fogstate.hpp>
#include <components/esm3/loadcell.hpp>
#include <components/files/memorystream.hpp>
#include <components/misc/constants.hpp>
#include <components/sceneutil/depth.hpp>
#include <components/sceneutil/lightmanager.hpp>
#include <components/sceneutil/nodecallback.hpp>
#include <components/sceneutil/rtt.hpp>
#include <components/sceneutil/shadow.hpp>
#include <components/sceneutil/visitor.hpp>
#include <components/settings/values.hpp>
#include <components/stereo/multiview.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwworld/cellstore.hpp"

#include "util.hpp"
#include "vismask.hpp"

namespace
{
    float square(float val)
    {
        return val * val;
    }

    int checkedMapIndex(double value)
    {
        if (!std::isfinite(value) || value < std::numeric_limits<int>::min()
            || value > std::numeric_limits<int>::max())
            throw std::runtime_error("Local-map coordinate exceeds the integer grid");
        return static_cast<int>(value);
    }

    std::pair<int, int> divideIntoSegments(const osg::BoundingBox& bounds, int mapSize)
    {
        if (!bounds.valid())
            return {0, 0};
        const int segsX = checkedMapIndex(std::ceil((double(bounds.xMax()) - bounds.xMin()) / mapSize));
        const int segsY = checkedMapIndex(std::ceil((double(bounds.yMax()) - bounds.yMin()) / mapSize));
        return { segsX, segsY };
    }
}

namespace MWRender
{
    class LocalMapRenderToTexture : public SceneUtil::RTTNode
    {
    public:
        LocalMapRenderToTexture(osg::Node* sceneRoot, int res, int mapWorldSize, float x, float y,
            const osg::Vec3d& upVector, float zmin, float zmax);

        void setDefaults(osg::Camera* camera) override;

        osg::Node* mSceneRoot;
        osg::Matrix mProjectionMatrix;
        osg::Matrix mViewMatrix;
        bool mActive;
    };

    class CameraLocalUpdateCallback
        : public SceneUtil::NodeCallback<CameraLocalUpdateCallback, LocalMapRenderToTexture*>
    {
    public:
        void operator()(LocalMapRenderToTexture* node, osg::NodeVisitor* nv);
    };

    LocalMap::LocalMap(osg::Group* root)
        : LocalMap(root, static_cast<int>(
              Settings::map().mLocalMapResolution * MWBase::Environment::get().getWindowManager()->getScalingFactor()))
    {
    }

    LocalMap::LocalMap(osg::Group* root, int mapResolution)
        : mRoot(root)
        , mMapResolution(mapResolution)
        , mMapWorldSize(Constants::CellSizeInUnits)
        , mCellDistance(Constants::CellGridRadius)
        , mAngle(0.f)
        , mInterior(false)
    {
        SceneUtil::FindByNameVisitor find("Scene Root");
        mRoot->accept(find);
        mSceneRoot = find.mFoundNode;
        if (!mSceneRoot)
            throw std::runtime_error("no scene root found");
    }

    LocalMap::~LocalMap()
    {
        for (auto& rtt : mLocalMapRTTs)
            mRoot->removeChild(rtt);
    }

    const osg::Vec2f LocalMap::rotatePoint(const osg::Vec2f& point, const osg::Vec2f& center, const float angle) const
    {
        return osg::Vec2f(
            std::cos(angle) * (point.x() - center.x()) - std::sin(angle) * (point.y() - center.y()) + center.x(),
            std::sin(angle) * (point.x() - center.x()) + std::cos(angle) * (point.y() - center.y()) + center.y());
    }

    void LocalMap::clear()
    {
        ++mInteriorRevision;
        for (const auto& rtt : mLocalMapRTTs)
            mRoot->removeChild(rtt);
        mLocalMapRTTs.clear();
        mExteriorSegments.clear();
        mInteriorSegments.clear();
        mInteriorSize = {0, 0};
        mInterior = false;
    }

    void LocalMap::saveFogOfWar(MWWorld::CellStore* cell) const
    {
        if (!mInterior)
        {
            const auto it
                = mExteriorSegments.find(std::make_pair(cell->getCell()->getGridX(), cell->getCell()->getGridY()));
            if (it == mExteriorSegments.end())
                return;
            const MapSegment& segment = it->second;

            if (segment.mFogOfWarImage && segment.mHasFogState)
            {
                auto fog = std::make_unique<ESM::FogState>();
                fog->mFogTextures.emplace_back();

                segment.saveFogOfWar(fog->mFogTextures.back());

                cell->setFog(std::move(fog));
            }
        }
        else
        {
            auto segments = divideIntoSegments(mBounds, mMapWorldSize);

            auto fog = std::make_unique<ESM::FogState>();

            fog->mBounds.mMinX = mBounds.xMin();
            fog->mBounds.mMaxX = mBounds.xMax();
            fog->mBounds.mMinY = mBounds.yMin();
            fog->mBounds.mMaxY = mBounds.yMax();
            fog->mNorthMarkerAngle = mAngle;
            fog->mCenterX = mCenter.x();
            fog->mCenterY = mCenter.y();

            fog->mFogTextures.reserve(mInteriorSegments.size());

            for (const auto& [coords, segment] : mInteriorSegments)
            {
                const auto [x, y] = coords;
                if (x < 0 || y < 0 || x >= segments.first || y >= segments.second || !segment.mHasFogState)
                    continue;
                ESM::FogTexture& texture = fog->mFogTextures.emplace_back();
                segment.saveFogOfWar(texture);
                texture.mX = x;
                texture.mY = y;
            }

            cell->setFog(std::move(fog));
        }
    }

    void LocalMap::setupRenderToTexture(
        int segmentX, int segmentY, float left, float top, const osg::Vec3d& upVector, float zmin, float zmax)
    {
        mLocalMapRTTs.emplace_back(
            new LocalMapRenderToTexture(mSceneRoot, mMapResolution, mMapWorldSize, left, top, upVector, zmin, zmax));

        mRoot->addChild(mLocalMapRTTs.back());

        MapSegment& segment = mInterior ? mInteriorSegments[std::make_pair(segmentX, segmentY)]
                                        : mExteriorSegments[std::make_pair(segmentX, segmentY)];
        segment.mMapTexture = static_cast<osg::Texture2D*>(mLocalMapRTTs.back()->getColorTexture(nullptr));
    }

    void LocalMap::requestMap(const MWWorld::CellStore* cell)
    {
        if (!cell->isExterior())
        {
            requestInteriorMap(cell);
            return;
        }

        int cellX = cell->getCell()->getGridX();
        int cellY = cell->getCell()->getGridY();

        MapSegment& segment = mExteriorSegments[std::make_pair(cellX, cellY)];
        const std::uint8_t neighbourFlags = getExteriorNeighbourFlags(cellX, cellY);
        if (segment.mLastRenderNeighbourFlags != 0
            && (segment.mLastRenderNeighbourFlags & neighbourFlags) == neighbourFlags)
            return;
        requestExteriorMap(cell, segment);
        segment.mLastRenderNeighbourFlags = neighbourFlags;
    }

    void LocalMap::addCell(MWWorld::CellStore* cell)
    {
        if (cell->isExterior())
            mExteriorSegments.emplace(
                std::make_pair(cell->getCell()->getGridX(), cell->getCell()->getGridY()), MapSegment{});
    }

    void LocalMap::removeExteriorCell(int x, int y)
    {
        mExteriorSegments.erase({ x, y });
    }

    void LocalMap::removeCell(MWWorld::CellStore* cell)
    {
        saveFogOfWar(cell);

        if (!cell->isExterior())
            clear();
    }

    osg::ref_ptr<osg::Texture2D> LocalMap::getMapTexture(int x, int y)
    {
        if (mInterior)
        {
            const auto* segment = ensureInteriorSegment(x, y, true);
            return segment ? segment->mMapTexture : nullptr;
        }
        auto& segments(mInterior ? mInteriorSegments : mExteriorSegments);
        SegmentMap::iterator found = segments.find(std::make_pair(x, y));
        if (found == segments.end())
            return osg::ref_ptr<osg::Texture2D>();
        else
            return found->second.mMapTexture;
    }

    osg::ref_ptr<osg::Texture2D> LocalMap::getFogOfWarTexture(int x, int y)
    {
        if (mInterior)
        {
            const auto* segment = ensureInteriorSegment(x, y, false);
            return segment ? segment->mFogOfWarTexture : nullptr;
        }
        auto& segments(mInterior ? mInteriorSegments : mExteriorSegments);
        SegmentMap::iterator found = segments.find(std::make_pair(x, y));
        if (found == segments.end())
            return osg::ref_ptr<osg::Texture2D>();
        else
            return found->second.mFogOfWarTexture;
    }

    void LocalMap::cleanupCameras()
    {
        auto it = mLocalMapRTTs.begin();
        while (it != mLocalMapRTTs.end())
        {
            if (!(*it)->mActive)
            {
                mRoot->removeChild(*it);
                it = mLocalMapRTTs.erase(it);
            }
            else
                it++;
        }
    }

    void LocalMap::requestExteriorMap(const MWWorld::CellStore* cell, MapSegment& segment)
    {
        mInterior = false;

        const int x = cell->getCell()->getGridX();
        const int y = cell->getCell()->getGridY();

        osg::BoundingSphere bound = mSceneRoot->getBound();
        float zmin = bound.center().z() - bound.radius();
        float zmax = bound.center().z() + bound.radius();

        setupRenderToTexture(x, y, float(x) * mMapWorldSize + mMapWorldSize / 2.f, float(y) * mMapWorldSize + mMapWorldSize / 2.f,
            osg::Vec3d(0, 1, 0), zmin, zmax);

        if (segment.mFogOfWarImage != nullptr)
            return;

        if (cell->getFog() && !cell->getFog()->mFogTextures.empty())
            segment.loadFogOfWar(cell->getFog()->mFogTextures.back());
        else
            segment.initFogOfWar();
    }

    static osg::Vec2f getNorthVector(const MWWorld::CellStore* cell)
    {
        MWWorld::ConstPtr northmarker = cell->searchConst(ESM::RefId::stringRefId("northmarker"));

        if (northmarker.isEmpty())
            return osg::Vec2f(0, 1);

        osg::Quat orient(-northmarker.getRefData().getPosition().rot[2], osg::Vec3f(0, 0, 1));
        osg::Vec3f dir = orient * osg::Vec3f(0, 1, 0);
        osg::Vec2f d(dir.x(), dir.y());
        return d;
    }

    void LocalMap::requestInteriorMap(const MWWorld::CellStore* cell)
    {
        osg::ComputeBoundsVisitor computeBoundsVisitor;
        computeBoundsVisitor.setTraversalMask(Mask_Scene | Mask_Terrain | Mask_Object | Mask_Static);
        mSceneRoot->accept(computeBoundsVisitor);

        osg::BoundingBox bounds = computeBoundsVisitor.getBoundingBox();

        // If we're in an empty cell, bail out
        // The operations in this function are only valid for finite bounds
        if (!bounds.valid() || bounds.radius2() == 0.0)
            return;

        ++mInteriorRevision;
        mInterior = true;
        mExteriorSegments.clear();

        mBounds = bounds;

        // Get the cell's NorthMarker rotation. This is used to rotate the entire map.
        osg::Vec2f north = getNorthVector(cell);

        mAngle = std::atan2(north.x(), north.y());

        // Rotate the cell and merge the rotated corners to the bounding box
        osg::Vec2f origCenter(bounds.center().x(), bounds.center().y());
        osg::Vec3f origCorners[8];
        for (int i = 0; i < 8; ++i)
            origCorners[i] = mBounds.corner(i);

        for (int i = 0; i < 8; ++i)
        {
            osg::Vec3f corner = origCorners[i];
            osg::Vec2f corner2d(corner.x(), corner.y());
            corner2d = rotatePoint(corner2d, origCenter, mAngle);
            mBounds.expandBy(osg::Vec3f(corner2d.x(), corner2d.y(), 0));
        }

        // Do NOT change padding! This will break older savegames.
        // If the padding really needs to be changed, then it must be saved in the ESM::FogState and
        // assume the old (500) value as default for older savegames.
        const float padding = 500.0f;

        // Apply a little padding
        mBounds.set(mBounds._min - osg::Vec3f(padding, padding, 0.f), mBounds._max + osg::Vec3f(padding, padding, 0.f));

        float zMin = mBounds.zMin();
        float zMax = mBounds.zMax();
        mCenter = osg::Vec2f(mBounds.center().x(), mBounds.center().y());

        // If there is fog state in the CellStore (e.g. when it came from a savegame) we need to do some checks
        // to see if this state is still valid.
        // Both the cell bounds and the NorthMarker rotation could be changed by the content files or exchanged models.
        // If they changed by too much then parts of the interior might not be covered by the map anymore.
        // The following code detects this, and discards the CellStore's fog state if it needs to.
        int xOffset = 0;
        int yOffset = 0;
        if (const ESM::FogState* fog = cell->getFog())
        {
            if (std::abs(mAngle - fog->mNorthMarkerAngle) < osg::DegreesToRadians(5.f))
            {
                // Expand mBounds so the saved textures fit the same grid
                if (fog->mBounds.mMinX < mBounds.xMin())
                {
                    mBounds.xMin() = fog->mBounds.mMinX;
                }
                else if (fog->mBounds.mMinX > mBounds.xMin())
                {
                    double diff = double(fog->mBounds.mMinX) - mBounds.xMin();
                    xOffset = checkedMapIndex(std::ceil(diff / mMapWorldSize));
                    mBounds.xMin() = static_cast<float>(double(fog->mBounds.mMinX) - double(xOffset) * mMapWorldSize);
                }
                if (fog->mBounds.mMinY < mBounds.yMin())
                {
                    mBounds.yMin() = fog->mBounds.mMinY;
                }
                else if (fog->mBounds.mMinY > mBounds.yMin())
                {
                    double diff = double(fog->mBounds.mMinY) - mBounds.yMin();
                    yOffset = checkedMapIndex(std::ceil(diff / mMapWorldSize));
                    mBounds.yMin() = static_cast<float>(double(fog->mBounds.mMinY) - double(yOffset) * mMapWorldSize);
                }
                if (fog->mBounds.mMaxX > mBounds.xMax())
                    mBounds.xMax() = fog->mBounds.mMaxX;
                if (fog->mBounds.mMaxY > mBounds.yMax())
                    mBounds.yMax() = fog->mBounds.mMaxY;

                if (xOffset != 0 || yOffset != 0)
                    Log(Debug::Warning) << "Warning: expanding fog by " << xOffset << ", " << yOffset;

                mAngle = fog->mNorthMarkerAngle;
                mCenter.x() = fog->mCenterX;
                mCenter.y() = fog->mCenterY;
            }
        }

        mInteriorSize = divideIntoSegments(mBounds, mMapWorldSize);
        mInteriorNorth = osg::Vec3d(north.x(), north.y(), 0.f);
        mInteriorZMin = zMin;
        mInteriorZMax = zMax;
        for (auto& [coords, segment] : mInteriorSegments)
            segment.mMapTexture = nullptr;

        // Retain actual saved fog fragments, not every cell in the bounds.
        // Unvisited fragments must remain available to queries and future saves.
        if (const ESM::FogState* fog = cell->getFog())
            for (const auto& texture : fog->mFogTextures)
            {
                const auto x = std::int64_t(texture.mX) + xOffset;
                const auto y = std::int64_t(texture.mY) + yOffset;
                if (x < 0 || y < 0 || x >= mInteriorSize.first || y >= mInteriorSize.second)
                    continue;
                auto& segment = mInteriorSegments[{static_cast<int>(x), static_cast<int>(y)}];
                if (!segment.mFogOfWarImage)
                    segment.loadFogOfWar(texture);
            }
    }

    LocalMap::MapSegment* LocalMap::ensureInteriorSegment(int x, int y, bool render)
    {
        if (!mInterior || x < 0 || y < 0 || x >= mInteriorSize.first || y >= mInteriorSize.second)
            return nullptr;
        auto& segment = mInteriorSegments[{x, y}];
        if (!segment.mFogOfWarImage)
            segment.initFogOfWar();
        if (render && !segment.mMapTexture)
        {
            const osg::Vec2f center(
                static_cast<float>(double(mBounds.xMin()) + double(mMapWorldSize) * (double(x) + 0.5)),
                static_cast<float>(double(mBounds.yMin()) + double(mMapWorldSize) * (double(y) + 0.5)));
            const osg::Quat cameraOrient(mAngle, osg::Vec3d(0, 0, -1));
            const osg::Vec2f relative = center - mCenter;
            const osg::Vec3f rotated = cameraOrient * osg::Vec3f(relative.x(), relative.y(), 0);
            const osg::Vec2f position = osg::Vec2f(rotated.x(), rotated.y()) + mCenter;
            setupRenderToTexture(x, y, position.x(), position.y(), mInteriorNorth, mInteriorZMin, mInteriorZMax);
        }
        return &segment;
    }

    void LocalMap::worldToInteriorMapPosition(osg::Vec2f pos, float& nX, float& nY, int& x, int& y) const
    {
        pos = rotatePoint(pos, mCenter, mAngle);

        osg::Vec2f min(mBounds.xMin(), mBounds.yMin());

        const int nextX = checkedMapIndex(std::ceil((double(pos.x()) - min.x()) / mMapWorldSize) - 1);
        const int nextY = checkedMapIndex(std::ceil((double(pos.y()) - min.y()) / mMapWorldSize) - 1);
        x = nextX;
        y = nextY;

        nX = static_cast<float>((double(pos.x()) - min.x() - double(mMapWorldSize) * x) / mMapWorldSize);
        nY = 1.0f - static_cast<float>((double(pos.y()) - min.y() - double(mMapWorldSize) * y) / mMapWorldSize);
    }

    osg::Vec2f LocalMap::interiorMapToWorldPosition(float nX, float nY, int x, int y) const
    {
        osg::Vec2f min(mBounds.xMin(), mBounds.yMin());
        osg::Vec2f pos(mMapWorldSize * (nX + x) + min.x(), mMapWorldSize * (1.0f - nY + y) + min.y());

        pos = rotatePoint(pos, mCenter, -mAngle);
        return pos;
    }

    bool LocalMap::isPositionExplored(float nX, float nY, int x, int y)
    {
        if (!std::isfinite(nX) || !std::isfinite(nY))
            return false;
        auto& segments(mInterior ? mInteriorSegments : mExteriorSegments);
        const auto found = segments.find(std::make_pair(x, y));
        if (found == segments.end())
            return false;
        const MapSegment& segment = found->second;
        if (!segment.mFogOfWarImage)
            return false;

        nX = std::clamp(nX, 0.f, 1.f);
        nY = std::clamp(nY, 0.f, 1.f);

        int texU = static_cast<int>((sFogOfWarResolution - 1) * nX);
        int texV = static_cast<int>((sFogOfWarResolution - 1) * nY);

        const std::uint32_t clr
            = reinterpret_cast<const uint32_t*>(segment.mFogOfWarImage->data())[texV * sFogOfWarResolution + texU];
        uint8_t alpha = (clr >> 24);
        return alpha < 200;
    }

    osg::Group* LocalMap::getRoot()
    {
        return mRoot;
    }

    void LocalMap::updatePlayer(const osg::Vec3f& position, const osg::Quat& orientation, float& u, float& v, int& x,
        int& y, osg::Vec3f& direction)
    {
        // retrieve the x,y grid coordinates the player is in
        osg::Vec2f pos(position.x(), position.y());

        if (mInterior)
        {
            worldToInteriorMapPosition(pos, u, v, x, y);

            osg::Quat cameraOrient(mAngle, osg::Vec3(0, 0, -1));
            direction = orientation * cameraOrient.inverse() * osg::Vec3f(0, 1, 0);
        }
        else
        {
            direction = orientation * osg::Vec3f(0, 1, 0);

            const int nextX = checkedMapIndex(std::ceil(double(pos.x()) / mMapWorldSize) - 1);
            const int nextY = checkedMapIndex(std::ceil(double(pos.y()) / mMapWorldSize) - 1);
            x = nextX;
            y = nextY;

            // convert from world coordinates to texture UV coordinates
            u = static_cast<float>(std::abs((double(pos.x()) - double(mMapWorldSize) * x) / mMapWorldSize));
            v = 1.0f - static_cast<float>(std::abs((double(pos.y()) - double(mMapWorldSize) * y) / mMapWorldSize));
        }

        // explore radius (squared)
        const float exploreRadius = 0.17f * (sFogOfWarResolution - 1); // explore radius from 0 to sFogOfWarResolution-1
        const float sqrExploreRadius = square(exploreRadius);
        const float exploreRadiusUV = exploreRadius / sFogOfWarResolution; // explore radius from 0 to 1 (UV space)

        // change the affected fog of war textures (in a 3x3 grid around the player)
        for (int mx = -mCellDistance; mx <= mCellDistance; ++mx)
        {
            for (int my = -mCellDistance; my <= mCellDistance; ++my)
            {
                // is this texture affected at all?
                bool affected = false;
                if (mx == 0 && my == 0) // the player is always in the center of the 3x3 grid
                    affected = true;
                else
                {
                    bool affectsX = (mx > 0) ? (u + exploreRadiusUV > 1) : (u - exploreRadiusUV < 0);
                    bool affectsY = (my > 0) ? (v + exploreRadiusUV > 1) : (v - exploreRadiusUV < 0);
                    affected = (affectsX && (my == 0)) || (affectsY && mx == 0) || (affectsX && affectsY);
                }

                if (!affected)
                    continue;

                const auto wideX = std::int64_t(x) + mx;
                const auto wideY = std::int64_t(y) - my;
                if (wideX < std::numeric_limits<int>::min() || wideX > std::numeric_limits<int>::max()
                    || wideY < std::numeric_limits<int>::min() || wideY > std::numeric_limits<int>::max())
                    continue;
                const int texX = static_cast<int>(wideX);
                const int texY = static_cast<int>(wideY);

                if (mInterior)
                    ensureInteriorSegment(texX, texY, true);
                auto& segments(mInterior ? mInteriorSegments : mExteriorSegments);
                const auto found = segments.find(std::make_pair(texX, texY));
                if (found == segments.end())
                    continue;
                MapSegment& segment = found->second;

                if (!segment.mFogOfWarImage || !segment.mMapTexture)
                    continue;

                std::uint32_t* data = reinterpret_cast<std::uint32_t*>(segment.mFogOfWarImage->data());
                bool changed = false;
                for (int texV = 0; texV < sFogOfWarResolution; ++texV)
                {
                    for (int texU = 0; texU < sFogOfWarResolution; ++texU)
                    {
                        float sqrDist = square((texU + mx * (sFogOfWarResolution - 1)) - u * (sFogOfWarResolution - 1))
                            + square((texV + my * (sFogOfWarResolution - 1)) - v * (sFogOfWarResolution - 1));

                        const std::uint8_t alpha = std::min<std::uint8_t>(*data >> 24,
                            static_cast<std::uint8_t>(std::clamp(sqrDist / sqrExploreRadius, 0.f, 1.f) * 255));
                        std::uint32_t val = static_cast<std::uint32_t>(alpha << 24);
                        if (*data != val)
                        {
                            *data = val;
                            changed = true;
                        }

                        ++data;
                    }
                }

                if (changed)
                {
                    segment.mHasFogState = true;
                    segment.mFogOfWarImage->dirty();
                }
            }
        }
    }

    std::uint8_t LocalMap::getExteriorNeighbourFlags(int cellX, int cellY) const
    {
        constexpr std::tuple<NeighbourCellFlag, int, int> flags[] = {
            { NeighbourCellTopLeft, -1, -1 },
            { NeighbourCellTopCenter, 0, -1 },
            { NeighbourCellTopRight, 1, -1 },
            { NeighbourCellMiddleLeft, -1, 0 },
            { NeighbourCellMiddleRight, 1, 0 },
            { NeighbourCellBottomLeft, -1, 1 },
            { NeighbourCellBottomCenter, 0, 1 },
            { NeighbourCellBottomRight, 1, 1 },
        };
        std::uint8_t result = 0;
        for (const auto& [flag, dx, dy] : flags)
        {
            const auto x = std::int64_t(cellX) + dx;
            const auto y = std::int64_t(cellY) + dy;
            if (x < std::numeric_limits<int>::min() || x > std::numeric_limits<int>::max()
                || y < std::numeric_limits<int>::min() || y > std::numeric_limits<int>::max())
                continue;
            auto it = mExteriorSegments.find(std::pair(static_cast<int>(x), static_cast<int>(y)));
            if (it != mExteriorSegments.end() && it->second.mMapTexture)
                result |= flag;
        }
        return result;
    }

    MyGUI::IntRect LocalMap::getInteriorGrid() const
    {
        return { -1, -1, mInteriorSize.first, mInteriorSize.second };
    }

    void LocalMap::MapSegment::createFogOfWarTexture()
    {
        if (mFogOfWarTexture)
            return;
        mFogOfWarTexture = new osg::Texture2D;
        // TODO: synchronize access? for now, the worst that could happen is the draw thread jumping a frame ahead.
        // mFogOfWarTexture->setDataVariance(osg::Object::DYNAMIC);
        mFogOfWarTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        mFogOfWarTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        mFogOfWarTexture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        mFogOfWarTexture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        mFogOfWarTexture->setUnRefImageDataAfterApply(false);
        mFogOfWarTexture->setImage(mFogOfWarImage);
    }

    void LocalMap::MapSegment::initFogOfWar()
    {
        mFogOfWarImage = new osg::Image;
        // Assign a PixelBufferObject for asynchronous transfer of data to the GPU
        mFogOfWarImage->setPixelBufferObject(new osg::PixelBufferObject);
        mFogOfWarImage->allocateImage(sFogOfWarResolution, sFogOfWarResolution, 1, GL_RGBA, GL_UNSIGNED_BYTE);
        assert(mFogOfWarImage->isDataContiguous());
        std::vector<uint32_t> data;
        data.resize(sFogOfWarResolution * sFogOfWarResolution, 0xff000000);

        memcpy(mFogOfWarImage->data(), data.data(), data.size() * 4);

        createFogOfWarTexture();
    }

    void LocalMap::MapSegment::loadFogOfWar(const ESM::FogTexture& esm)
    {
        static_assert(sFogOfWarResolution == ESM::FogTexture::Resolution);
        const std::vector<char>& data = esm.mImageData;
        if (data.empty())
        {
            initFogOfWar();
            return;
        }

        if (esm.mPrepared)
        {
            if (!esm.mPreparedImage) return; // Admitted unreadable-image skip.
            mFogOfWarImage = esm.mPreparedImage;
            createFogOfWarTexture();
            mHasFogState = true;
            return;
        }

        osgDB::ReaderWriter* readerwriter = osgDB::Registry::instance()->getReaderWriterForExtension("png");
        if (!readerwriter)
        {
            Log(Debug::Error) << "Error: Unable to load fog, can't find a png ReaderWriter";
            return;
        }

        Files::IMemStream in(data.data(), data.size());

        osgDB::ReaderWriter::ReadResult result = readerwriter->readImage(in);
        if (!result.success())
        {
            Log(Debug::Error) << "Error: Failed to read fog: " << result.message() << " code " << result.status();
            return;
        }

        if (!result.getImage() || !ESM::isUsableFogImage(*result.getImage()))
        {
            Log(Debug::Warning) << "Skipping local-map fog with unsupported image shape or channels";
            return;
        }
        mFogOfWarImage = result.getImage();
        mFogOfWarImage->flipVertical();
        mFogOfWarImage->dirty();

        createFogOfWarTexture();
        mHasFogState = true;
    }

    void LocalMap::MapSegment::saveFogOfWar(ESM::FogTexture& fog) const
    {
        if (!mFogOfWarImage)
            return;

        std::ostringstream ostream;

        osgDB::ReaderWriter* readerwriter = osgDB::Registry::instance()->getReaderWriterForExtension("png");
        if (!readerwriter)
        {
            Log(Debug::Error) << "Error: Unable to write fog, can't find a png ReaderWriter";
            return;
        }

        // extra flips are unfortunate, but required for compatibility with older versions
        mFogOfWarImage->flipVertical();
        osgDB::ReaderWriter::WriteResult result = readerwriter->writeImage(*mFogOfWarImage, ostream);
        if (!result.success())
        {
            Log(Debug::Error) << "Error: Unable to write fog: " << result.message() << " code " << result.status();
            return;
        }
        mFogOfWarImage->flipVertical();

        std::string data = ostream.str();
        fog.mImageData = std::vector<char>(data.begin(), data.end());
    }

    LocalMapRenderToTexture::LocalMapRenderToTexture(osg::Node* sceneRoot, int res, int mapWorldSize, float x, float y,
        const osg::Vec3d& upVector, float zmin, float zmax)
        : RTTNode(res, res, 0, false, 0, StereoAwareness::Unaware_MultiViewShaders, shouldAddMSAAIntermediateTarget())
        , mSceneRoot(sceneRoot)
        , mActive(true)
    {
        setNodeMask(Mask_RenderToTexture);

        if (SceneUtil::AutoDepth::isReversed())
            mProjectionMatrix = SceneUtil::getReversedZProjectionMatrixAsOrtho(
                -mapWorldSize / 2, mapWorldSize / 2, -mapWorldSize / 2, mapWorldSize / 2, 5, (zmax - zmin) + 10);
        else
            mProjectionMatrix.makeOrtho(
                -mapWorldSize / 2, mapWorldSize / 2, -mapWorldSize / 2, mapWorldSize / 2, 5, (zmax - zmin) + 10);

        mViewMatrix.makeLookAt(osg::Vec3d(x, y, zmax + 5), osg::Vec3d(x, y, zmin), upVector);

        setUpdateCallback(new CameraLocalUpdateCallback);
        setDepthBufferInternalFormat(GL_DEPTH24_STENCIL8);
    }

    void LocalMapRenderToTexture::setDefaults(osg::Camera* camera)
    {
        // Disable small feature culling, it's not going to be reliable for this camera
        osg::Camera::CullingMode cullingMode
            = (osg::Camera::DEFAULT_CULLING | osg::Camera::FAR_PLANE_CULLING) & ~(osg::Camera::SMALL_FEATURE_CULLING);
        camera->setCullingMode(cullingMode);

        SceneUtil::setCameraClearDepth(camera);
        camera->setComputeNearFarMode(osg::Camera::DO_NOT_COMPUTE_NEAR_FAR);
        camera->setReferenceFrame(osg::Camera::ABSOLUTE_RF_INHERIT_VIEWPOINT);
        camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT, osg::Camera::PIXEL_BUFFER_RTT);
        camera->setClearColor(osg::Vec4(0.f, 0.f, 0.f, 1.f));
        camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        camera->setRenderOrder(osg::Camera::PRE_RENDER);

        camera->setCullMask(Mask_Scene | Mask_SimpleWater | Mask_Terrain | Mask_Object | Mask_Static);
        camera->setCullMaskLeft(Mask_Scene | Mask_SimpleWater | Mask_Terrain | Mask_Object | Mask_Static);
        camera->setCullMaskRight(Mask_Scene | Mask_SimpleWater | Mask_Terrain | Mask_Object | Mask_Static);
        camera->setNodeMask(Mask_RenderToTexture);
        camera->setProjectionMatrix(mProjectionMatrix);
        camera->setViewMatrix(mViewMatrix);

        auto* stateset = camera->getOrCreateStateSet();

        stateset->setAttribute(new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::FILL),
            osg::StateAttribute::OVERRIDE);
        stateset->addUniform(new osg::Uniform("projectionMatrix", static_cast<osg::Matrixf>(mProjectionMatrix)),
            osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);

        if (Stereo::getMultiview())
            Stereo::setMultiviewMatrices(stateset, { mProjectionMatrix, mProjectionMatrix });

        // assign large value to effectively turn off fog
        // shaders don't respect glDisable(GL_FOG)
        osg::ref_ptr<osg::Fog> fog(new osg::Fog);
        fog->setStart(10000000);
        fog->setEnd(10000000);
        stateset->setAttributeAndModes(fog, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);

        // turn of sky blending
        stateset->addUniform(new osg::Uniform("far", 10000000.0f));
        stateset->addUniform(new osg::Uniform("skyBlendingStart", 8000000.0f));
        stateset->addUniform(new osg::Uniform("screenRes", osg::Vec2f{ 1, 1 }));

        osg::ref_ptr<SceneUtil::Light> light = new SceneUtil::Light;
        light->setPosition(osg::Vec4(-0.3f, -0.3f, 0.7f, 0.f));
        light->setDiffuse(osg::Vec4(0.7f, 0.7f, 0.7f, 1.f));
        light->setAmbient(osg::Vec4(0.3f, 0.3f, 0.3f, 1.f));
        light->setSpecular(osg::Vec4(0, 0, 0, 0));
        light->setConstantAttenuation(1.f);
        light->setLinearAttenuation(0.f);
        light->setQuadraticAttenuation(0.f);

        SceneUtil::ShadowManager::instance().disableShadowsForStateSet(*stateset);

        // override sun for local map
        SceneUtil::configureStateSetSunOverride(light, stateset);

        camera->addChild(mSceneRoot);
    }

    void CameraLocalUpdateCallback::operator()(LocalMapRenderToTexture* node, osg::NodeVisitor* nv)
    {
        if (!node->mActive)
            node->setNodeMask(0);

        node->mActive = false;

        // Rtt-nodes do not forward update traversal to their cameras so we can traverse safely.
        // Traverse in case there are nested callbacks.
        traverse(node, nv);
    }

}
