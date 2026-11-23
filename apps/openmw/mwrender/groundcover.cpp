#include "groundcover.hpp"

#include "groundcoverplacement.hpp"

#include <array>
#include <numbers>
#include <optional>
#include <span>

#include <osg/AlphaFunc>
#include <osg/BlendFunc>
#include <osg/ComputeBoundsVisitor>
#include <osg/Geometry>
#include <osg/Program>
#include <osg/VertexAttribDivisor>
#include <osgUtil/CullVisitor>

#include <components/debug/debuglog.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/loadland.hpp>
#include <components/esm3/readerscache.hpp>
#include <components/esm4/loadgras.hpp>
#include <components/esm4/loadland.hpp>
#include <components/esm4/loadltex.hpp>
#include <components/esm4/loadwrld.hpp>
#include <components/misc/convert.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/sceneutil/lightmanager.hpp>
#include <components/sceneutil/nodecallback.hpp>
#include <components/settings/values.hpp>
#include <components/shader/shadermanager.hpp>
#include <components/terrain/quadtreenode.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/groundcoverstore.hpp"
#include "../mwworld/worldspaceutils.hpp"

#include "terrainstorage.hpp"
#include "vismask.hpp"

namespace MWRender
{
    namespace
    {
        using value_type = osgUtil::CullVisitor::value_type;

        // From OSG's CullVisitor.cpp
        inline value_type distance(const osg::Vec3& coord, const osg::Matrix& matrix)
        {
            return -((value_type)coord[0] * (value_type)matrix(0, 2) + (value_type)coord[1] * (value_type)matrix(1, 2)
                + (value_type)coord[2] * (value_type)matrix(2, 2) + matrix(3, 2));
        }

        inline osg::Matrix computeInstanceMatrix(
            const Groundcover::GroundcoverEntry& entry, const osg::Vec3& chunkPosition)
        {
            return osg::Matrix::scale(entry.mWidthScale, entry.mWidthScale, entry.mScale)
                * osg::Matrix(Misc::Convert::makeOsgQuat(entry.mPos))
                * osg::Matrix::translate(entry.mPos.asVec3() - chunkPosition);
        }

        class InstancedComputeNearFarCullCallback : public osg::DrawableCullCallback
        {
        public:
            explicit InstancedComputeNearFarCullCallback(std::span<const Groundcover::GroundcoverEntry> instances,
                const osg::Vec3& chunkPosition, const osg::BoundingBox& instanceBounds)
                : mInstanceMatrices()
                , mInstanceBounds(instanceBounds)
            {
                mInstanceMatrices.reserve(instances.size());
                for (const Groundcover::GroundcoverEntry& instance : instances)
                    mInstanceMatrices.emplace_back(computeInstanceMatrix(instance, chunkPosition));
            }

            bool cull(osg::NodeVisitor* nv, osg::Drawable* drawable, osg::RenderInfo* renderInfo) const override
            {
                osgUtil::CullVisitor& cullVisitor = *nv->asCullVisitor();
                osg::CullSettings::ComputeNearFarMode cnfMode = cullVisitor.getComputeNearFarMode();
                const osg::BoundingBox& boundingBox = drawable->getBoundingBox();
                osg::RefMatrix& matrix = *cullVisitor.getModelViewMatrix();

                if (cnfMode != osg::CullSettings::COMPUTE_NEAR_FAR_USING_PRIMITIVES
                    && cnfMode != osg::CullSettings::COMPUTE_NEAR_USING_PRIMITIVES)
                    return false;

                if (drawable->isCullingActive() && cullVisitor.isCulled(boundingBox))
                    return true;

                osg::Vec3 lookVector = cullVisitor.getLookVectorLocal();
                unsigned int bbCornerFar
                    = (lookVector.x() >= 0 ? 1 : 0) | (lookVector.y() >= 0 ? 2 : 0) | (lookVector.z() >= 0 ? 4 : 0);
                unsigned int bbCornerNear = (~bbCornerFar) & 7;
                value_type dNear = distance(boundingBox.corner(bbCornerNear), matrix);
                value_type dFar = distance(boundingBox.corner(bbCornerFar), matrix);

                if (dNear > dFar)
                    std::swap(dNear, dFar);

                if (dFar < 0)
                    return true;

                value_type computedZNear = cullVisitor.getCalculatedNearPlane();
                value_type computedZFar = cullVisitor.getCalculatedFarPlane();

                if (dNear < computedZNear || dFar > computedZFar)
                {
                    osg::Polytope frustum;
                    osg::Polytope::ClippingMask resultMask
                        = cullVisitor.getCurrentCullingSet().getFrustum().getResultMask();
                    if (resultMask)
                    {
                        // Other objects are likely cheaper and should let us skip all but a few groundcover instances
                        cullVisitor.computeNearPlane();
                        computedZNear = cullVisitor.getCalculatedNearPlane();
                        computedZFar = cullVisitor.getCalculatedFarPlane();

                        if (dNear < computedZNear)
                        {
                            dNear = computedZNear;
                            for (const auto& instanceMatrix : mInstanceMatrices)
                            {
                                osg::Matrix fullMatrix = instanceMatrix * matrix;
                                osg::Vec3d instanceLookVector(-fullMatrix(0, 2), -fullMatrix(1, 2), -fullMatrix(2, 2));
                                unsigned int instanceBbCornerFar = (instanceLookVector.x() >= 0 ? 1 : 0)
                                    | (instanceLookVector.y() >= 0 ? 2 : 0) | (instanceLookVector.z() >= 0 ? 4 : 0);
                                unsigned int instanceBbCornerNear = (~instanceBbCornerFar) & 7;
                                value_type instanceDNear
                                    = distance(mInstanceBounds.corner(instanceBbCornerNear), fullMatrix);
                                value_type instanceDFar
                                    = distance(mInstanceBounds.corner(instanceBbCornerFar), fullMatrix);

                                if (instanceDNear > instanceDFar)
                                    std::swap(instanceDNear, instanceDFar);

                                if (instanceDFar < 0 || instanceDNear > dNear)
                                    continue;

                                frustum.setAndTransformProvidingInverse(
                                    cullVisitor.getProjectionCullingStack().back().getFrustum(), fullMatrix);
                                osg::Polytope::PlaneList planes;
                                osg::Polytope::ClippingMask selectorMask = 0x1;
                                for (const auto& plane : frustum.getPlaneList())
                                {
                                    if (resultMask & selectorMask)
                                        planes.push_back(plane);
                                    selectorMask <<= 1;
                                }

                                value_type newNear
                                    = cullVisitor.computeNearestPointInFrustum(fullMatrix, planes, *drawable);
                                dNear = std::min(dNear, newNear);
                            }
                            if (dNear < computedZNear)
                                cullVisitor.setCalculatedNearPlane(dNear);
                        }

                        if (cnfMode == osg::CullSettings::COMPUTE_NEAR_FAR_USING_PRIMITIVES && dFar > computedZFar)
                        {
                            dFar = computedZFar;
                            for (const auto& instanceMatrix : mInstanceMatrices)
                            {
                                osg::Matrix fullMatrix = instanceMatrix * matrix;
                                osg::Vec3d instanceLookVector(-fullMatrix(0, 2), -fullMatrix(1, 2), -fullMatrix(2, 2));
                                unsigned int instanceBbCornerFar = (instanceLookVector.x() >= 0 ? 1 : 0)
                                    | (instanceLookVector.y() >= 0 ? 2 : 0) | (instanceLookVector.z() >= 0 ? 4 : 0);
                                unsigned int instanceBbCornerNear = (~instanceBbCornerFar) & 7;
                                value_type instanceDNear
                                    = distance(mInstanceBounds.corner(instanceBbCornerNear), fullMatrix);
                                value_type instanceDFar
                                    = distance(mInstanceBounds.corner(instanceBbCornerFar), fullMatrix);

                                if (instanceDNear > instanceDFar)
                                    std::swap(instanceDNear, instanceDFar);

                                if (instanceDFar < 0 || instanceDFar < dFar)
                                    continue;

                                frustum.setAndTransformProvidingInverse(
                                    cullVisitor.getProjectionCullingStack().back().getFrustum(), fullMatrix);
                                osg::Polytope::PlaneList planes;
                                osg::Polytope::ClippingMask selectorMask = 0x1;
                                for (const auto& plane : frustum.getPlaneList())
                                {
                                    if (resultMask & selectorMask)
                                        planes.push_back(plane);
                                    selectorMask <<= 1;
                                }

                                value_type newFar = cullVisitor.computeFurthestPointInFrustum(
                                    instanceMatrix * matrix, planes, *drawable);
                                dFar = std::max(dFar, newFar);
                            }
                            if (dFar > computedZFar)
                                cullVisitor.setCalculatedFarPlane(dFar);
                        }
                    }
                }

                return false;
            }

        private:
            std::vector<osg::Matrix> mInstanceMatrices;
            osg::BoundingBox mInstanceBounds;
        };

        class GrassMeshCopy : public osg::CopyOp
        {
        public:
            explicit GrassMeshCopy(bool native)
                : osg::CopyOp(DEEP_COPY_NODES | DEEP_COPY_DRAWABLES | DEEP_COPY_USERDATA | DEEP_COPY_PRIMITIVES
                      | DEEP_COPY_ARRAYS)
                , mNative(native)
            {
            }

            using osg::CopyOp::operator();

            osg::Array* operator()(const osg::Array* source) const override
            {
                // The OSG geometry copy constructor attaches buffer objects to unbuffered
                // arrays. Chunk workers may share an already buffered static mesh, but any
                // new bindings must belong to the clone.
                if (mNative && source && source->getVertexBufferObject()
                    && source->getDataVariance() != osg::Object::DYNAMIC)
                    return const_cast<osg::Array*>(source);
                auto* copy = osg::CopyOp::operator()(source);
                if (mNative && copy)
                    copy->setVertexBufferObject(new osg::VertexBufferObject);
                return copy;
            }

        private:
            bool mNative;
        };

        class InstancingVisitor : public osg::NodeVisitor
        {
        public:
            explicit InstancingVisitor(
                std::span<const Groundcover::GroundcoverEntry> instances, osg::Vec3f& chunkPosition)
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
                , mInstances(instances)
                , mChunkPosition(chunkPosition)
            {
                const auto count = static_cast<unsigned>(instances.size());
                mOffsets = new osg::Vec4Array(count);
                mRotations = new osg::Vec4Array(count);
                mLighting = new osg::Vec4Array(count);
                osg::ref_ptr<osg::VertexBufferObject> buffer = new osg::VertexBufferObject;
                mOffsets->setVertexBufferObject(buffer);
                mRotations->setVertexBufferObject(buffer);
                mLighting->setVertexBufferObject(buffer);
                for (unsigned i = 0; i < count; ++i)
                {
                    const auto& entry = instances[i];
                    (*mOffsets)[i] = osg::Vec4f(entry.mPos.asVec3() - chunkPosition, entry.mScale);
                    (*mRotations)[i] = osg::Vec4f(entry.mPos.asRotationVec3(), entry.mWidthScale);
                    (*mLighting)[i] = osg::Vec4f(entry.mGroundNormal, entry.mBrightness);
                }
            }

            void apply(osg::Group& group) override
            {
                for (unsigned int i = 0; i < group.getNumChildren();)
                {
                    if (group.getChild(i)->asDrawable() && !group.getChild(i)->asGeometry())
                        group.removeChild(i);
                    else
                        ++i;
                }
                traverse(group);
            }

            void apply(osg::Geometry& geom) override
            {
                for (unsigned int i = 0; i < geom.getNumPrimitiveSets(); ++i)
                {
                    geom.getPrimitiveSet(i)->setNumInstances(static_cast<int>(mInstances.size()));
                }

                osg::BoundingBox box;
                const osg::BoundingBox originalBox = geom.getBoundingBox();
                const float radius = originalBox.radius() + originalBox.center().length();
                for (std::size_t i = 0; i < mInstances.size(); ++i)
                {
                    const auto& entry = mInstances[i];
                    // Include the off-center mesh origin and movement of animated blades.
                    const float instanceRadius = radius * std::max(entry.mScale, entry.mWidthScale) * 1.1f;
                    box.expandBy(osg::BoundingSphere(entry.mPos.asVec3() - mChunkPosition, instanceRadius));
                }
                geom.setInitialBound(box);

                // Display lists do not support instancing in OSG 3.4
                geom.setUseDisplayList(false);
                geom.setUseVertexBufferObjects(true);

                geom.setVertexAttribArray(5, mLighting.get(), osg::Array::BIND_PER_VERTEX);
                geom.setVertexAttribArray(6, mOffsets.get(), osg::Array::BIND_PER_VERTEX);
                geom.setVertexAttribArray(7, mRotations.get(), osg::Array::BIND_PER_VERTEX);

                if (Settings::shadows().mComputeSceneBounds.get() == "primitives")
                    geom.addCullCallback(
                        new InstancedComputeNearFarCullCallback(mInstances, mChunkPosition, originalBox));
            }

        private:
            std::span<const Groundcover::GroundcoverEntry> mInstances;
            osg::Vec3f mChunkPosition;
            osg::ref_ptr<osg::Vec4Array> mOffsets;
            osg::ref_ptr<osg::Vec4Array> mRotations;
            osg::ref_ptr<osg::Vec4Array> mLighting;
        };

        class DensityCalculator
        {
        public:
            DensityCalculator(float density)
                : mDensity(density)
            {
            }

            bool isInstanceEnabled()
            {
                if (mDensity >= 1.f)
                    return true;

                mCurrentGroundcover += mDensity;
                if (mCurrentGroundcover < 1.f)
                    return false;

                mCurrentGroundcover -= 1.f;

                return true;
            }
            void reset() { mCurrentGroundcover = 0.f; }

        private:
            float mCurrentGroundcover = 0.f;
            float mDensity = 0.f;
        };

        class ViewDistanceCallback : public SceneUtil::NodeCallback<ViewDistanceCallback>
        {
        public:
            ViewDistanceCallback(float dist, const osg::BoundingBox& box)
                : mViewDistance(dist)
                , mBox(box)
            {
            }
            void operator()(osg::Node* node, osg::NodeVisitor* nv)
            {
                if (Terrain::distance(mBox, nv->getEyePoint()) <= mViewDistance)
                    traverse(node, nv);
            }

        private:
            float mViewDistance;
            osg::BoundingBox mBox;
        };

        inline bool isInChunkBorders(ESM::CellRef& ref, osg::Vec2f& minBound, osg::Vec2f& maxBound)
        {
            osg::Vec2f size = maxBound - minBound;
            if (size.x() >= 1 && size.y() >= 1)
                return true;

            osg::Vec3f pos = ref.mPos.asVec3();
            osg::Vec3f cellPos = pos / ESM::Land::REAL_SIZE;
            if ((minBound.x() > std::floor(minBound.x()) && cellPos.x() < minBound.x())
                || (minBound.y() > std::floor(minBound.y()) && cellPos.y() < minBound.y())
                || (maxBound.x() < std::ceil(maxBound.x()) && cellPos.x() >= maxBound.x())
                || (maxBound.y() < std::ceil(maxBound.y()) && cellPos.y() >= maxBound.y()))
                return false;

            return true;
        }

        std::uint32_t mix(std::uint32_t value)
        {
            value ^= value >> 16;
            value *= 0x7feb352du;
            value ^= value >> 15;
            value *= 0x846ca68bu;
            value ^= value >> 16;
            return value;
        }

        float randomUnit(std::uint32_t seed)
        {
            return static_cast<float>(mix(seed) & 0x00ffffffu) / static_cast<float>(0x01000000u);
        }

        // Sample one of the four neighboring paint vertices with bilinear probabilities.
        // The cumulative weights use the same base remainder as the terrain blendmaps.
        class GrassTextureSampler
        {
            static constexpr int sSide = ESM4::Land::sVertsPerSide / 2 + 1;
            struct Layer
            {
                ESM::FormId mTexture;
                std::array<float, sSide * sSide> mCumulative{};
            };
            ESM::FormId mBase;
            std::vector<Layer> mLayers;

        public:
            explicit GrassTextureSampler(const ESM4::Land::Texture& texture)
                : mBase(ESM::FormId::fromUint32(texture.base.formId))
            {
                mLayers.reserve(texture.layers.size());
                for (const auto& paint : texture.layers)
                {
                    auto& layer = mLayers.emplace_back();
                    layer.mTexture = ESM::FormId::fromUint32(paint.texture.formId);
                    for (const auto& entry : paint.data)
                        if (entry.position < layer.mCumulative.size())
                            layer.mCumulative[entry.position] = std::clamp(entry.opacity, 0.f, 1.f);
                    if (mLayers.size() > 1)
                    {
                        const auto& previous = mLayers[mLayers.size() - 2];
                        for (std::size_t i = 0; i < layer.mCumulative.size(); ++i)
                            layer.mCumulative[i] += previous.mCumulative[i];
                    }
                }
            }

            ESM::FormId sample(float x, float y, std::uint32_t seed) const
            {
                if (mLayers.empty())
                    return mBase;
                const int vx = std::min(sSide - 1, static_cast<int>(x * (sSide - 1) + randomUnit(seed)));
                const int vy = std::min(sSide - 1, static_cast<int>(y * (sSide - 1) + randomUnit(seed + 1)));
                const int index = vy * sSide + vx;
                const float roll = randomUnit(seed + 2) * std::max(1.f, mLayers.back().mCumulative[index]);
                for (const auto& layer : mLayers)
                    if (roll < layer.mCumulative[index])
                        return layer.mTexture;
                return mBase;
            }
        };

        struct GrassPalette
        {
            struct Species
            {
                const ESM4::Grass* mGrass;
                VFS::Path::Normalized mModel;
                float mCumulative;
            };
            std::vector<Species> mSpecies;

            GrassPalette(const MWWorld::ESMStore& store, ESM::FormId textureId)
            {
                const auto* texture = store.get<ESM4::LandTexture>().search(textureId);
                if (!texture)
                    return;
                float total = 0.f;
                for (const auto id : texture->mGrass)
                {
                    const auto* grass = store.get<ESM4::Grass>().search(id);
                    if (!grass || grass->mModel.empty() || !grass->mData.density)
                        continue;
                    total += std::min<int>(grass->mData.density, 100);
                    mSpecies.push_back(
                        { grass, Misc::ResourceHelpers::correctMeshPath(grass->mModel.getNormalized()), total });
                }
            }

            const Species* select(float roll) const
            {
                if (mSpecies.empty())
                    return nullptr;
                roll *= std::max(100.f, mSpecies.back().mCumulative);
                for (const auto& species : mSpecies)
                    if (roll < species.mCumulative)
                        return &species;
                return nullptr;
            }
        };
    }

    osg::ref_ptr<osg::Node> Groundcover::getChunk(float size, const osg::Vec2f& center, unsigned char lod,
        unsigned int lodFlags, bool activeGrid, const osg::Vec3f& viewPoint, bool compile)
    {
        if (lod > getMaxLodLevel() || getViewDistance() <= 0.f || mDensity <= 0.f)
            return nullptr;
        GroundcoverChunkId id = std::make_tuple(center, size);
        osg::ref_ptr<osg::Object> obj = mCache->getRefFromObjectCache(id);
        if (obj)
            return static_cast<osg::Node*>(obj.get());
        else
        {
            InstanceMap instances;
            InstanceMap nearInstances;
            if (ESM::isEsm4Ext(mWorldspace))
                collectEsm4Instances(instances, nearInstances, size, center);
            else
                collectInstances(instances, size, center);
            if (ESM::isEsm4Ext(mWorldspace) && !mLoggedNativeInstances.load(std::memory_order_relaxed))
            {
                std::size_t instanceCount = 0;
                for (const auto& [_, entries] : instances)
                    instanceCount += entries.size();
                for (const auto& [_, entries] : nearInstances)
                    instanceCount += entries.size();
                if (instanceCount != 0 && !mLoggedNativeInstances.exchange(true))
                {
                    Log(Debug::Info) << "M9 native groundcover populated a terrain chunk with " << instanceCount
                                     << " GRAS instances";
                }
            }
            osg::ref_ptr<osg::Group> node = new osg::Group;
            if (!instances.empty())
                node->addChild(createChunk(instances, center, 1.f));
            if (!nearInstances.empty())
                node->addChild(createChunk(nearInstances, center, 0.5f));
            node->setNodeMask(Mask_Groundcover);
            mCache->addEntryToObjectCache(id, node.get());
            return node;
        }
    }

    Groundcover::Groundcover(Resource::SceneManager* sceneManager, float density, float viewDistance,
        const MWWorld::GroundcoverStore& store, TerrainStorage* terrainStorage, ESM::RefId worldspace)
        : GenericResourceManager<GroundcoverChunkId>(nullptr, Settings::cells().mCacheExpiryDelay)
        , mSceneManager(sceneManager)
        , mDensity(density)
        , mStateset(new osg::StateSet)
        , mGroundcoverStore(store)
        , mTerrainStorage(terrainStorage)
        , mWorldspace(worldspace)
    {
        setViewDistance(viewDistance);
        // MGE uses default alpha settings for groundcover, so we can not rely on alpha properties
        // Force a unified alpha handling instead of data from meshes
        osg::ref_ptr<osg::AlphaFunc> alpha = new osg::AlphaFunc(osg::AlphaFunc::GEQUAL, 128.f / 255.f);
        mStateset->setAttributeAndModes(alpha.get(), osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
        mStateset->setAttributeAndModes(new osg::BlendFunc, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
        mStateset->setRenderBinDetails(0, "RenderBin", osg::StateSet::OVERRIDE_RENDERBIN_DETAILS);
        mStateset->setAttribute(new osg::VertexAttribDivisor(5, 1));
        mStateset->setAttribute(new osg::VertexAttribDivisor(6, 1));
        mStateset->setAttribute(new osg::VertexAttribDivisor(7, 1));

        mProgramTemplate = mSceneManager->getShaderManager().getProgramTemplate()
            ? Shader::ShaderManager::cloneProgram(mSceneManager->getShaderManager().getProgramTemplate())
            : osg::ref_ptr<osg::Program>(new osg::Program);
        mProgramTemplate->addBindAttribLocation("aGrassLighting", 5);
        mProgramTemplate->addBindAttribLocation("aOffset", 6);
        mProgramTemplate->addBindAttribLocation("aRotation", 7);

        if (ESM::isEsm4Ext(mWorldspace))
        {
            const MWWorld::ESMStore& esmStore = *MWBase::Environment::get().getESMStore();
            Log(Debug::Info) << "M9 native groundcover enabled for " << mWorldspace.toDebugString() << " with "
                             << esmStore.get<ESM4::Grass>().getSize() << " GRAS records at density " << mDensity;
        }
    }

    Groundcover::~Groundcover() = default;

    void Groundcover::collectInstances(InstanceMap& instances, float size, const osg::Vec2f& center)
    {
        osg::Vec2f minBound = (center - osg::Vec2f(size / 2.f, size / 2.f));
        osg::Vec2f maxBound = (center + osg::Vec2f(size / 2.f, size / 2.f));
        DensityCalculator calculator(mDensity);
        ESM::ReadersCache readers;
        osg::Vec2i startCell = osg::Vec2i(static_cast<int>(std::floor(center.x() - size / 2.f)),
            static_cast<int>(std::floor(center.y() - size / 2.f)));
        for (int cellX = startCell.x(); cellX < startCell.x() + size; ++cellX)
        {
            for (int cellY = startCell.y(); cellY < startCell.y() + size; ++cellY)
            {
                ESM::Cell cell;
                mGroundcoverStore.initCell(cell, cellX, cellY);
                if (cell.mContextList.empty())
                    continue;

                calculator.reset();
                std::map<ESM::RefNum, ESM::CellRef> refs;
                for (size_t i = 0; i < cell.mContextList.size(); ++i)
                {
                    const std::size_t index = static_cast<std::size_t>(cell.mContextList[i].index);
                    const ESM::ReadersCache::BusyItem reader = readers.get(index);
                    cell.restore(*reader, i);
                    ESM::CellRef ref;
                    bool deleted = false;
                    while (cell.getNextRef(*reader, ref, deleted))
                    {
                        if (!deleted && refs.find(ref.mRefNum) == refs.end() && !calculator.isInstanceEnabled())
                            deleted = true;
                        if (!deleted && !isInChunkBorders(ref, minBound, maxBound))
                            deleted = true;

                        if (deleted)
                        {
                            refs.erase(ref.mRefNum);
                            continue;
                        }
                        refs[ref.mRefNum] = std::move(ref);
                    }
                }

                for (auto& [refNum, cellRef] : refs)
                {
                    const VFS::Path::NormalizedView model = mGroundcoverStore.getGroundcoverModel(cellRef.mRefID);
                    if (model.empty())
                        continue;
                    auto it = instances.find(model);
                    if (it == instances.end())
                        it = instances.emplace_hint(it, VFS::Path::Normalized(model), std::vector<GroundcoverEntry>());
                    it->second.emplace_back(std::move(cellRef));
                }
            }
        }
    }

    void Groundcover::collectEsm4Instances(
        InstanceMap& farInstances, InstanceMap& nearInstances, float size, const osg::Vec2f& center)
    {
        if (mTerrainStorage == nullptr)
            return;

        constexpr float cellSize = ESM4::Land::sRealSize;
        // Oblivion_default.ini uses iMinGrassSize=80. Fit the lattice exactly to cell edges.
        static constexpr int samplesPerSide = ESM4::Land::sRealSize / 80;
        constexpr float spacing = cellSize / samplesPerSide;
        const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();
        const auto& worlds = store.get<ESM4::World>();
        const auto* waterWorld = MWWorld::getWorldspaceFeature(worlds, mWorldspace, ESM4::World::UseFlag_Water);
        const float waterHeight = waterWorld ? waterWorld->mWaterLevel : std::numeric_limits<float>::lowest();
        std::map<ESM::FormId, GrassPalette> palettes;
        const osg::Vec2f minBound = center - osg::Vec2f(size / 2.f, size / 2.f);
        const osg::Vec2f maxBound = center + osg::Vec2f(size / 2.f, size / 2.f);
        const int minCellX = static_cast<int>(std::floor(minBound.x()));
        const int minCellY = static_cast<int>(std::floor(minBound.y()));
        const int maxCellX = static_cast<int>(std::ceil(maxBound.x()));
        const int maxCellY = static_cast<int>(std::ceil(maxBound.y()));

        for (int cellX = minCellX; cellX < maxCellX; ++cellX)
        {
            for (int cellY = minCellY; cellY < maxCellY; ++cellY)
            {
                const auto land = mTerrainStorage->getLand({ cellX, cellY, mWorldspace });
                const auto* data = land ? land->getData(ESM4::Land::LAND_VHGT) : nullptr;
                if (!data)
                    continue;

                std::array<std::optional<GrassTextureSampler>, 4> quadrants;
                const auto first
                    = [](float edge) { return std::max(0, static_cast<int>(std::floor(edge * samplesPerSide))); };
                const auto last = [](float edge) {
                    return std::min(samplesPerSide, static_cast<int>(std::ceil(edge * samplesPerSide)));
                };
                const int endX = last(maxBound.x() - cellX);
                const int endY = last(maxBound.y() - cellY);
                for (int y = first(minBound.y() - cellY); y < endY; ++y)
                {
                    for (int x = first(minBound.x() - cellX); x < endX; ++x)
                    {
                        const std::uint32_t seed = mix(static_cast<std::uint32_t>(cellX) * 0x8da6b343u
                            ^ static_cast<std::uint32_t>(cellY) * 0xd8163841u
                            ^ static_cast<std::uint32_t>(y * samplesPerSide + x));
                        if (randomUnit(seed) >= mDensity)
                            continue;
                        const float centerX = (x + 0.5f) / samplesPerSide;
                        const float centerY = (y + 0.5f) / samplesPerSide;
                        const int qx = centerX >= 0.5f;
                        const int qy = centerY >= 0.5f;
                        const int quadrant = qy * 2 + qx;
                        auto& sampler = quadrants[quadrant];
                        if (!sampler)
                            sampler.emplace(data->getEsm4Texture(quadrant));
                        const auto texture = sampler->sample(centerX * 2 - qx, centerY * 2 - qy, seed + 1);
                        const auto& palette = palettes.try_emplace(texture, store, texture).first->second;
                        const auto* species = palette.select(randomUnit(seed + 4));
                        if (!species)
                            continue;

                        const auto& traits = species->mGrass->mData;
                        const float jitter = std::clamp(traits.positionRange, 0.f, spacing * 0.5f) / cellSize;
                        const float localX = centerX + (randomUnit(seed + 6) * 2.f - 1.f) * jitter;
                        const float localY = centerY + (randomUnit(seed + 7) * 2.f - 1.f) * jitter;
                        if (localX < minBound.x() - cellX || localX >= maxBound.x() - cellX
                            || localY < minBound.y() - cellY || localY >= maxBound.y() - cellY)
                            continue;

                        const auto [height, normal] = sampleGrassSurface(data->getHeights(), localX, localY);
                        if (!acceptsGrassWaterDistance(
                                traits.waterDistApplication, traits.distanceFromWater, height - waterHeight))
                            continue;
                        const float slope = osg::RadiansToDegrees(std::acos(std::clamp(normal.z(), 0.f, 1.f)));
                        if (slope < traits.minSlope || slope > traits.maxSlope)
                            continue;

                        ESM::Position position;
                        position.pos[0] = (cellX + localX) * cellSize;
                        position.pos[1] = (cellY + localY) * cellSize;
                        position.pos[2] = height;
                        if (traits.flags & 0x04)
                        {
                            // The shader applies pitch and roll after the random heading.
                            position.rot[0] = std::asin(std::clamp(normal.y(), -1.f, 1.f));
                            position.rot[1] = -std::atan2(normal.x(), normal.z());
                        }
                        position.rot[2] = randomUnit(seed + 8) * 2.f * std::numbers::pi_v<float>;
                        const float scale
                            = 1.f + (randomUnit(seed + 9) * 2.f - 1.f) * std::clamp(traits.heightRange, 0.f, 1.f);
                        // A stable quarter of the cover extends to the configured horizon.
                        auto& instances = randomUnit(seed + 10) < 0.25f ? farInstances : nearInstances;
                        auto& entry = instances[species->mModel].emplace_back(position, scale);
                        entry.mWidthScale = (traits.flags & 0x02) ? scale : 1.f;
                        entry.mGroundNormal = (traits.flags & 0x01) ? osg::Vec3f() : normal;
                        entry.mBrightness = 1.f - randomUnit(seed + 11) * std::clamp(traits.colorRange, 0.f, 1.f);
                    }
                }
            }
        }
    }

    osg::ref_ptr<osg::Node> Groundcover::createChunk(
        InstanceMap& instances, const osg::Vec2f& center, float rangeFactor)
    {
        osg::ref_ptr<osg::Group> group = new osg::Group;
        const float cellWorldSize = ESM::isEsm4Ext(mWorldspace) && mTerrainStorage != nullptr
            ? mTerrainStorage->getCellWorldSize(mWorldspace)
            : ESM::Land::REAL_SIZE;
        osg::Vec3f worldCenter = osg::Vec3f(center.x(), center.y(), 0) * cellWorldSize;
        for (const auto& [model, entries] : instances)
        {
            const osg::Node* temp = mSceneManager->getTemplate(model);
            osg::ref_ptr<osg::Node> node
                = static_cast<osg::Node*>(temp->clone(GrassMeshCopy(ESM::isEsm4Ext(mWorldspace))));

            // Keep link to original mesh to keep it in cache
            group->getOrCreateUserDataContainer()->addUserObject(new Resource::TemplateRef(temp));

            InstancingVisitor visitor(entries, worldCenter);
            node->accept(visitor);
            group->addChild(node);
        }

        osg::ComputeBoundsVisitor cbv;
        group->accept(cbv);
        osg::BoundingBox box = cbv.getBoundingBox();
        group->addCullCallback(new ViewDistanceCallback(getViewDistance() * rangeFactor, box));

        osg::ref_ptr<osg::StateSet> state = new osg::StateSet(*mStateset);
        state->addUniform(new osg::Uniform("groundcoverRangeFactor", rangeFactor));
        group->setStateSet(state);
        group->setNodeMask(Mask_Groundcover);
        if (Settings::groundcover().mPointLighting)
            group->addCullCallback(new SceneUtil::LightListCallback);
        mSceneManager->recreateShaders(group, "groundcover", mProgramTemplate);
        mSceneManager->shareState(group);
        group->getBound();
        return group;
    }

    unsigned int Groundcover::getNodeMask()
    {
        return Mask_Groundcover;
    }

    void Groundcover::reportStats(unsigned int frameNumber, osg::Stats* stats) const
    {
        Resource::reportStats("Groundcover Chunk", frameNumber, mCache->getStats(), *stats);
    }
}
