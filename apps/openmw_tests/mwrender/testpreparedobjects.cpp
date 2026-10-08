#include <apps/openmw/mwworld/cellstore.hpp>
#include <apps/openmw/mwworld/esmstore.hpp>
#include <components/esm3/readerscache.hpp>
#include <components/esm3/loadcell.hpp>
#include <apps/openmw/mwrender/objects.hpp>
#include <apps/openmw/mwrender/localmap.hpp>
#include <apps/openmw/mwgui/localmapview.hpp>
#include <apps/openmw/mwgui/quickkeyresources.hpp>
#include <apps/openmw/mwrender/vismask.hpp>
#include <apps/openmw/mwclass/static.hpp>
#include <apps/openmw/mwworld/livecellref.hpp>
#include <components/esm3/loadstat.hpp>
#include <components/sceneutil/unrefqueue.hpp>
#include <components/sceneutil/shadow.hpp>
#include <components/settings/values.hpp>
#include <components/sceneutil/glextensions.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>
#include <SDL.h>
#include <components/sceneutil/positionattitudetransform.hpp>
#include <components/testing/util.hpp>
#include <osgDB/Registry>
#include <osgDB/ReaderWriter>
#include <sstream>
#include <limits>
#include <new>
#include <cstddef>
#include <apps/openmw/mwrender/animation.hpp>
#include <apps/openmw/mwrender/util.hpp>
#include <components/nif/niftypes.hpp>
#include <components/nifosg/matrixtransform.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/vfs/manager.hpp>

#include <gtest/gtest.h>
#include <osg/Group>
#include <osg/Geode>
#include <osg/ShapeDrawable>

namespace
{
    struct LocalMapGraphicsContext
    {
        SDL_Window* mWindow = nullptr;
        osg::ref_ptr<SDLUtil::GraphicsWindowSDL2> mContext;
        LocalMapGraphicsContext()
        {
            if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
                throw std::runtime_error(SDL_GetError());
            mWindow = SDL_CreateWindow("Local map integration", 0, 0, 32, 32, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
            if (!mWindow)
                throw std::runtime_error(SDL_GetError());
            osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
            traits->width = traits->height = 32;
            traits->inheritedWindowData = new SDLUtil::GraphicsWindowSDL2::WindowData(mWindow);
            mContext = new SDLUtil::GraphicsWindowSDL2(traits, SDLUtil::Disabled);
            if (!mContext->valid() || !mContext->realize() || !mContext->makeCurrent())
                throw std::runtime_error("Could not create the local-map integration graphics context");
            osg::ref_ptr<SceneUtil::GetGLExtensionsOperation> extensions = new SceneUtil::GetGLExtensionsOperation;
            (*extensions)(mContext);
        }
        ~LocalMapGraphicsContext()
        {
            mContext = nullptr;
            SDL_DestroyWindow(mWindow);
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }
    };

    TEST(LocalMapCoordinatesTest, LargeRepresentableExteriorCoordinatesKeepNormalizedPosition)
    {
        osg::ref_ptr<osg::Group> root = new osg::Group;
        osg::ref_ptr<osg::Group> scene = new osg::Group;
        scene->setName("Scene Root");
        root->addChild(scene);
        MWRender::LocalMap map(root, 32);
        for (float position : {4294967296.f, -4294967296.f, 8192.f, -8192.f, 0.f})
        {
            SCOPED_TRACE(position);
            float u = -1, v = -1;
            int x = 0, y = 0;
            osg::Vec3f direction;
            map.updatePlayer({position, position, 0}, osg::Quat(), u, v, x, y, direction);
            EXPECT_FLOAT_EQ(u, 1.f);
            EXPECT_FLOAT_EQ(v, 0.f);
            EXPECT_EQ(x, static_cast<int>(double(position) / 8192) - 1);
            EXPECT_EQ(y, x);
            EXPECT_EQ(root->getNumChildren(), 1u);
        }
    }

    TEST(LocalMapCoordinatesTest, ExteriorBoundaryAndInvalidCoordinatesDoNotOverflowOrPublishOutputs)
    {
        osg::ref_ptr<osg::Group> root = new osg::Group;
        osg::ref_ptr<osg::Group> scene = new osg::Group;
        scene->setName("Scene Root");
        root->addChild(scene);
        MWRender::LocalMap map(root, 32);
        float u = -1, v = -1;
        int x = 123, y = 456;
        osg::Vec3f direction;
        const float boundary = std::ldexp(8192.f, 31);
        ASSERT_NO_THROW(map.updatePlayer({boundary, boundary, 0}, osg::Quat(), u, v, x, y, direction));
        EXPECT_EQ(x, std::numeric_limits<int>::max());
        EXPECT_EQ(y, x);
        EXPECT_FLOAT_EQ(u, 1);
        EXPECT_FLOAT_EQ(v, 0);
        for (float invalid : {std::nextafter(boundary, std::numeric_limits<float>::infinity()), -boundary,
                 std::numeric_limits<float>::max(), std::numeric_limits<float>::infinity(),
                 std::numeric_limits<float>::quiet_NaN()})
        {
            u = -1; v = -2; x = 123; y = 456;
            EXPECT_THROW(map.updatePlayer({8192, invalid, 0}, osg::Quat(), u, v, x, y, direction), std::runtime_error);
            EXPECT_EQ(x, 123); EXPECT_EQ(y, 456);
            EXPECT_FLOAT_EQ(u, -1); EXPECT_FLOAT_EQ(v, -2);
        }
        const float inside = std::nextafter(8192.f, std::numeric_limits<float>::infinity());
        map.updatePlayer({inside, inside, 0}, osg::Quat(), u, v, x, y, direction);
        EXPECT_EQ(x, 1); EXPECT_EQ(y, 1);
        EXPECT_FLOAT_EQ(u, std::ldexp(1.f, -23));
        EXPECT_FLOAT_EQ(v, 1 - std::ldexp(1.f, -23));
    }

    TEST(LocalMapCoordinatesTest, ActualInteriorMapRetainsCoordinateRoundTripAndFogExploration)
    {
        osg::ref_ptr<osg::Group> root = new osg::Group;
        osg::ref_ptr<osg::Group> scene = new osg::Group;
        scene->setName("Scene Root");
        osg::ref_ptr<osg::Geode> geometry = new osg::Geode;
        geometry->setNodeMask(MWRender::Mask_Static);
        geometry->addDrawable(new osg::ShapeDrawable(new osg::Box(osg::Vec3(), 1024.f)));
        scene->addChild(geometry);
        root->addChild(scene);
        MWWorld::ESMStore store;
        ESM::ReadersCache readers;
        ESM::Cell record; record.blank(); record.mId = ESM::RefId::stringRefId("map arithmetic interior");
        record.mData.mFlags = ESM::Cell::Interior;
        MWWorld::CellStore cell{MWWorld::Cell(record), store, readers}; cell.load();
        // Local-map cameras use the engine's process-wide shadow service.
        static LocalMapGraphicsContext context;
        static Shader::ShaderManager shaders;
        shaders.setShaderPath(OPENMW_PROJECT_SOURCE_DIR "/files/shaders");
        static SceneUtil::ShadowManager shadows(scene, root, MWRender::Mask_Static,
            MWRender::Mask_Static, MWRender::Mask_Scene, Settings::shadows(), shaders);
        MWRender::LocalMap map(root, 32);
        const auto baseChildren = root->getNumChildren();
        map.requestMap(&cell);
        ASSERT_TRUE(map.getMapTexture(0, 0));
        ASSERT_TRUE(map.getFogOfWarTexture(0, 0));
        float u, v; int x, y;
        map.worldToInteriorMapPosition({4294967296.f, 4294967296.f}, u, v, x, y);
        EXPECT_EQ(x, 524288); EXPECT_EQ(y, 524288);
        EXPECT_FLOAT_EQ(u, 1012.f / 8192);
        EXPECT_FLOAT_EQ(v, 1 - 1012.f / 8192);
        const auto position = map.interiorMapToWorldPosition(u, v, x, y);
        EXPECT_FLOAT_EQ(position.x(), 4294967296.f); EXPECT_FLOAT_EQ(position.y(), 4294967296.f);
        osg::Vec3f direction;
        map.updatePlayer({0, 0, 0}, osg::Quat(), u, v, x, y, direction);
        EXPECT_TRUE(map.isPositionExplored(u, v, x, y));
        EXPECT_FALSE(map.isPositionExplored(std::numeric_limits<float>::quiet_NaN(), v, x, y));
        EXPECT_FALSE(map.isPositionExplored(u, std::numeric_limits<float>::infinity(), x, y));
        map.saveFogOfWar(&cell);
        ASSERT_TRUE(cell.getFog());
        ASSERT_EQ(cell.getFog()->mFogTextures.size(), 1u);
        EXPECT_EQ(cell.getFog()->mFogTextures.front().mX, 0);
        EXPECT_EQ(cell.getFog()->mFogTextures.front().mY, 0);
        const auto explored = *cell.getFog();

        // A saved fragment far from the current viewport must survive without
        // rendering the entire theoretical grid, including a trillion-cell map.
        for (float span : {65536.f, 8589934592.f})
        {
            SCOPED_TRACE(span);
            const auto oldRevision = map.getInteriorRevision();
            map.clear();
            EXPECT_NE(map.getInteriorRevision(), oldRevision);
            EXPECT_EQ(root->getNumChildren(), baseChildren);
            geometry->removeDrawables(0, geometry->getNumDrawables());
            geometry->addDrawable(new osg::ShapeDrawable(new osg::Box(osg::Vec3(), span, span, 1024.f)));
            auto retained = std::make_unique<ESM::FogState>(explored);
            retained->mBounds = {-span / 2 - 500.f, -span / 2 - 500.f,
                span / 2 + 500.f, span / 2 + 500.f};
            retained->mCenterX = retained->mCenterY = 0;
            retained->mFogTextures.front().mX = 6;
            retained->mFogTextures.front().mY = 7;
            cell.setFog(std::move(retained));
            const auto children = root->getNumChildren();
            map.requestMap(&cell);
            ASSERT_EQ(root->getNumChildren(), children);
            EXPECT_TRUE(map.isPositionExplored(1012.f / 8192, 1 - 1012.f / 8192, 6, 7));
            EXPECT_FALSE(map.getMapTexture(-1, 0));
            EXPECT_FALSE(map.getMapTexture(std::numeric_limits<int>::max(), 0));
            const auto texture = map.getMapTexture(0, 0);
            ASSERT_TRUE(texture);
            EXPECT_EQ(root->getNumChildren(), children + 1);
            EXPECT_EQ(map.getMapTexture(0, 0), texture);
            EXPECT_EQ(root->getNumChildren(), children + 1);
            ASSERT_TRUE(map.getFogOfWarTexture(0, 0));
            map.saveFogOfWar(&cell);
            ASSERT_EQ(cell.getFog()->mFogTextures.size(), 1u);
            EXPECT_EQ(cell.getFog()->mFogTextures.front().mX, 6);
            EXPECT_EQ(cell.getFog()->mFogTextures.front().mY, 7);
            map.updatePlayer({0, 0, 0}, osg::Quat(), u, v, x, y, direction);
            EXPECT_TRUE(map.isPositionExplored(u, v, x, y));
            EXPECT_LE(root->getNumChildren(), children + 5);
            map.saveFogOfWar(&cell);
            EXPECT_LE(cell.getFog()->mFogTextures.size(), 5u);
            EXPECT_TRUE(std::any_of(cell.getFog()->mFogTextures.begin(), cell.getFog()->mFogTextures.end(),
                [](const auto& fog) { return fog.mX == 6 && fog.mY == 7; }));
        }
        map.removeCell(&cell);
        EXPECT_EQ(root->getNumChildren(), baseChildren);
        EXPECT_FALSE(map.getMapTexture(x, y));
        EXPECT_FALSE(map.getFogOfWarTexture(x, y));
        EXPECT_FALSE(map.isPositionExplored(u, v, x, y));
    }

    class PreparedModelFailureRoot : public osg::Group
    {
    public:
        enum class Failure { None, Reject, InsertReject, InsertThrow };
        Failure mFailure = Failure::None;
        bool addChild(osg::Node* child) override
        {
            if (mFailure == Failure::Reject) return false;
            const bool result = osg::Group::addChild(child);
            if (mFailure == Failure::InsertReject) return false;
            if (mFailure == Failure::InsertThrow) throw std::runtime_error("scene insertion failed after parenting");
            return result;
        }
    };

    struct PreparedObjectModelTest : testing::Test
    {
        static std::string modelBytes()
        {
            osg::ref_ptr<osg::Group> model = new osg::Group;
            model->setName("Prepared actual model");
            std::ostringstream out;
            auto* writer = osgDB::Registry::instance()->getReaderWriterForExtension("osgt");
            if (!writer || !writer->writeNode(*model, out).success())
                throw std::runtime_error("could not write test-owned model");
            return out.str();
        }
        TestingOpenMW::VFSTestFile mFile{modelBytes()};
        std::unique_ptr<VFS::Manager> mVfs = TestingOpenMW::createTestVFS({
            {VFS::Path::NormalizedView("prepared.osgt"), &mFile}});
        Resource::ResourceSystem mResources{mVfs.get(), 0., nullptr};
        SceneUtil::UnrefQueue mUnref;
        osg::ref_ptr<PreparedModelFailureRoot> mRoot = new PreparedModelFailureRoot;
        static ESM::Cell modelCell()
        {
            ESM::Cell cell;
            cell.blank();
            cell.mId = ESM::RefId::stringRefId("PreparedObjectModelCell");
            cell.mData.mFlags = ESM::Cell::Interior;
            return cell;
        }
        MWWorld::ESMStore mStore;
        ESM::ReadersCache mReaders;
        // CellVariant borrows its source record; keep that record alive for
        // the complete CellStore lifetime instead of wrapping a local copy.
        ESM::Cell mCellRecord = modelCell();
        MWWorld::CellStore mCell{MWWorld::Cell(mCellRecord), mStore, mReaders};
        ESM::Static mBase;
        ESM::CellRef mReference;
        std::unique_ptr<MWWorld::LiveCellRef<ESM::Static>> mLive;
        MWWorld::Ptr mPtr;
        PreparedObjectModelTest()
        {
            MWClass::Static::registerSelf();
            mBase.blank();
            mReference.blank();
            mLive = std::make_unique<MWWorld::LiveCellRef<ESM::Static>>(mReference, &mBase);
            mCell.load();
            mPtr = MWWorld::Ptr(mLive.get(), &mCell);
        }
        static constexpr unsigned Mask = MWRender::Mask_Object;
        std::unique_ptr<MWRender::Objects::PreparedModel> prepare(MWRender::Objects& objects,
            const MWWorld::Ptr& ptr)
        { return objects.prepareModel(ptr, "prepared.osgt", osg::Quat(), Mask); }
    };

    TEST_F(PreparedObjectModelTest, CancellationBuildsPrivateActualModelWithoutReferenceOrScenePublication)
    {
        MWRender::Objects objects(&mResources, mRoot, mUnref);
        auto original = mResources.getSceneManager()->getTemplate(VFS::Path::NormalizedView("prepared.osgt"));
        const auto originalParents = original->getNumParents();
        {
            auto prepared = prepare(objects, mPtr);
            EXPECT_TRUE(objects.validatePreparedModel(*prepared));
            EXPECT_EQ(mPtr.getRefData().getBaseNode(), nullptr);
            EXPECT_EQ(objects.getAnimation(mPtr), nullptr);
            EXPECT_EQ(mRoot->getNumChildren(), 0u);
            EXPECT_EQ(mUnref.getSize(), 0u);
            EXPECT_EQ(mPtr.getCellRef().getCount(), 1);
        }
        EXPECT_EQ(original->getNumParents(), originalParents);
        EXPECT_EQ(mPtr.getRefData().getBaseNode(), nullptr);
        EXPECT_EQ(mRoot->getNumChildren(), 0u);
        EXPECT_THROW(objects.prepareModel(mPtr, "missing.nif", osg::Quat(), Mask), std::runtime_error);
        EXPECT_EQ(objects.getAnimation(mPtr), nullptr);
        EXPECT_EQ(mRoot->getNumChildren(), 0u);
    }

    TEST_F(PreparedObjectModelTest, PlacementCountScaleAndOwnerChangesRejectBeforeScenePublication)
    {
        MWRender::Objects objects(&mResources, mRoot, mUnref);
        osg::ref_ptr<osg::Group> foreignRoot = new osg::Group;
        MWRender::Objects foreign(&mResources, foreignRoot, mUnref);
        auto prepared = prepare(objects, mPtr);
        EXPECT_FALSE(foreign.commitModel(*prepared));
        mPtr.getCellRef().setCount(2);
        EXPECT_FALSE(objects.commitModel(*prepared));
        mPtr.getCellRef().setCount(1);
        const auto pos = mPtr.getRefData().getPosition();
        auto changed = pos;
        changed.pos[0] = 1;
        mPtr.getRefData().setPosition(changed);
        EXPECT_FALSE(objects.commitModel(*prepared));
        mPtr.getRefData().setPosition(pos);
        mPtr.getCellRef().setScale(2);
        EXPECT_FALSE(objects.commitModel(*prepared));
        mPtr.getCellRef().setScale(1);
        EXPECT_TRUE(objects.validatePreparedModel(*prepared));
        EXPECT_EQ(mRoot->getNumChildren(), 0u);
        changed.pos[0] = std::numeric_limits<float>::quiet_NaN();
        mPtr.getRefData().setPosition(changed);
        EXPECT_THROW(prepare(objects, mPtr), std::invalid_argument);
        mPtr.getRefData().setPosition(pos);
        osg::Quat bad;
        bad.x() = std::numeric_limits<double>::quiet_NaN();
        EXPECT_THROW(objects.prepareModel(mPtr, "", bad, Mask), std::invalid_argument);
        EXPECT_THROW(objects.prepareModel({}, "", osg::Quat(), Mask), std::invalid_argument);
        const MWWorld::Ptr orphan(mLive.get());
        EXPECT_THROW(objects.prepareModel(orphan, "", osg::Quat(), Mask), std::invalid_argument);
        EXPECT_EQ(mPtr.getRefData().getBaseNode(), nullptr);
        EXPECT_EQ(objects.getAnimation(mPtr), nullptr);
        EXPECT_EQ(mRoot->getNumChildren(), 0u);
    }

    TEST_F(PreparedObjectModelTest, SceneRejectionAndPostInsertionFailureRollBackBeforeRetry)
    {
        MWRender::Objects objects(&mResources, mRoot, mUnref);
        auto prepared = prepare(objects, mPtr);
        for (auto failure : {PreparedModelFailureRoot::Failure::Reject,
                 PreparedModelFailureRoot::Failure::InsertReject, PreparedModelFailureRoot::Failure::InsertThrow})
        {
            mRoot->mFailure = failure;
            EXPECT_THROW(objects.commitModel(*prepared), std::runtime_error);
            EXPECT_TRUE(objects.validatePreparedModel(*prepared));
            EXPECT_EQ(mRoot->getNumChildren(), 0u);
            EXPECT_EQ(mPtr.getRefData().getBaseNode(), nullptr);
            EXPECT_EQ(objects.getAnimation(mPtr), nullptr);
            EXPECT_EQ(mUnref.getSize(), 0u);
            EXPECT_EQ(mPtr.getCellRef().getCount(), 1);
        }
        mRoot->mFailure = PreparedModelFailureRoot::Failure::None;
        ASSERT_TRUE(objects.commitModel(*prepared));
        EXPECT_FALSE(objects.commitModel(*prepared));
        auto* node = mPtr.getRefData().getBaseNode();
        ASSERT_NE(node, nullptr);
        EXPECT_EQ(node->getNodeMask(), Mask);
        EXPECT_EQ(node->getNumParents(), 1u);
        ASSERT_NE(objects.getAnimation(mPtr), nullptr);
        EXPECT_EQ(objects.getAnimation(mPtr)->getObjectRoot()->getName(), "Prepared actual model");
        EXPECT_EQ(mRoot->getNumChildren(), 1u);
        ASSERT_TRUE(objects.removeObject(mPtr));
        EXPECT_EQ(mPtr.getRefData().getBaseNode(), nullptr);
        EXPECT_EQ(objects.getAnimation(mPtr), nullptr);
        EXPECT_EQ(node->getNumParents(), 0u);
        EXPECT_EQ(mUnref.getSize(), 1u);
    }

    TEST_F(PreparedObjectModelTest, SynchronousRollbackRemovesOnlyItsPublicationWithoutUnrefAllocation)
    {
        MWRender::Objects objects(&mResources, mRoot, mUnref);
        auto prepared = prepare(objects, mPtr);
        ASSERT_TRUE(objects.commitModel(*prepared));
        ASSERT_TRUE(objects.rollbackModelAdmission(*prepared));
        EXPECT_EQ(mRoot->getNumChildren(), 0u);
        EXPECT_EQ(mPtr.getRefData().getBaseNode(), nullptr);
        EXPECT_EQ(objects.getAnimation(mPtr), nullptr);
        EXPECT_EQ(mUnref.getSize(), 0u);
        EXPECT_EQ(mPtr.getCellRef().getCount(), 1);
        EXPECT_FALSE(objects.rollbackModelAdmission(*prepared));
        EXPECT_FALSE(objects.commitModel(*prepared));
        auto fresh = prepare(objects, mPtr);
        EXPECT_TRUE(objects.commitModel(*fresh));
        fresh.reset(); // Successful publication is owned by Objects.
        EXPECT_NE(objects.getAnimation(mPtr), nullptr);
        EXPECT_NE(mPtr.getRefData().getBaseNode(), nullptr);
    }

    TEST_F(PreparedObjectModelTest, RollbackPreservesExistingCellAndRejectsAReplacementModel)
    {
        MWRender::Objects objects(&mResources, mRoot, mUnref);
        MWWorld::LiveCellRef<ESM::Static> other(mReference, &mBase);
        const MWWorld::Ptr otherPtr(&other, &mCell);
        auto first = prepare(objects, otherPtr);
        ASSERT_TRUE(objects.commitModel(*first));
        auto* otherNode = otherPtr.getRefData().getBaseNode();
        auto second = prepare(objects, mPtr);
        ASSERT_TRUE(objects.commitModel(*second));
        EXPECT_EQ(mRoot->getChild(0)->asGroup()->getNumChildren(), 2u);
        ASSERT_TRUE(objects.rollbackModelAdmission(*second));
        EXPECT_EQ(mRoot->getNumChildren(), 1u);
        EXPECT_EQ(mRoot->getChild(0)->asGroup()->getNumChildren(), 1u);
        EXPECT_EQ(otherPtr.getRefData().getBaseNode(), otherNode);
        auto old = prepare(objects, mPtr);
        ASSERT_TRUE(objects.commitModel(*old));
        ASSERT_TRUE(objects.removeObject(mPtr));
        auto replacement = prepare(objects, mPtr);
        ASSERT_TRUE(objects.commitModel(*replacement));
        auto* replacementNode = mPtr.getRefData().getBaseNode();
        EXPECT_FALSE(objects.rollbackModelAdmission(*old));
        EXPECT_EQ(mPtr.getRefData().getBaseNode(), replacementNode);
        EXPECT_NE(objects.getAnimation(mPtr), nullptr);
        objects.removeObject(mPtr);
        objects.removeObject(otherPtr);
    }

    TEST_F(PreparedObjectModelTest, ReusedObjectsOwnerRejectsStaleTokenBeforeAccessingDeletedReference)
    {
        using Objects = MWRender::Objects;
        alignas(Objects) std::byte storage[sizeof(Objects)];
        auto* objects = new (storage) Objects(&mResources, mRoot, mUnref);
        auto retired = prepare(*objects, mPtr);
        objects->~Objects();
        mLive.reset();
        objects = new (storage) Objects(&mResources, mRoot, mUnref);
        EXPECT_FALSE(objects->validatePreparedModel(*retired));
        EXPECT_FALSE(objects->commitModel(*retired));
        EXPECT_FALSE(objects->rollbackModelAdmission(*retired));
        retired.reset();
        EXPECT_EQ(mRoot->getNumChildren(), 0u);
        objects->~Objects();
    }
}


TEST(LocalMapViewTest, InteriorGridNearIntegerLimitsAllocatesOnlyViewportTiles)
{
    const MyGUI::IntRect full{-1, -1, std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
    for (double center : {0., 1000000000.25, double(std::numeric_limits<int>::max())})
    {
        const auto grid = MWGui::LocalMapView::gridForViewport(full, center, center, 1024, 768, 256);
        EXPECT_LE(std::int64_t(grid.right) - grid.left + 1, 8);
        EXPECT_LE(std::int64_t(grid.bottom) - grid.top + 1, 7);
        EXPECT_LE(grid.left, center); EXPECT_GE(grid.right, center);
        EXPECT_LE(grid.top, center); EXPECT_GE(grid.bottom, center);
    }
}

TEST(LocalMapViewTest, SubcellFractionsSurviveLargeLogicalIndices)
{
    const MyGUI::IntRect grid{999999999, 999999999, 1000000003, 1000000003};
    const auto point = MWGui::LocalMapView::position(grid, 1000000000, 1000000000, .25f, .75f, 256);
    EXPECT_EQ(point.left, 320); EXPECT_EQ(point.top, 960);
}


TEST(LocalMapViewTest, SmallGridEdgesAndPanRebasingPreserveTheLogicalViewpoint)
{
    const MyGUI::IntRect small{-1, -1, 2, 3};
    EXPECT_EQ(MWGui::LocalMapView::gridForViewport(small, 0, 0, 1024, 768, 256), small);
    const MyGUI::IntRect full{-1, -1, std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
    auto grid = MWGui::LocalMapView::gridForViewport(full, 1000000000.25, 1000000000.25, 1024, 768, 256);
    MyGUI::IntPoint offset{-520, -480};
    for (const auto delta : {MyGUI::IntPoint{400, -400}, MyGUI::IntPoint{-700, 350}, MyGUI::IntPoint{100, 100}})
    {
        offset += delta;
        const double x = grid.left + (512. - offset.left) / 256;
        const double y = double(grid.bottom) + 1 - (384. - offset.top) / 256;
        const auto next = MWGui::LocalMapView::gridForViewport(full, x, y, 1024, 768, 256);
        offset.left += MWGui::LocalMapView::pixel((double(next.left) - grid.left) * 256);
        offset.top += MWGui::LocalMapView::pixel((double(grid.bottom) - next.bottom) * 256);
        EXPECT_DOUBLE_EQ(next.left + (512. - offset.left) / 256, x);
        EXPECT_DOUBLE_EQ(double(next.bottom) + 1 - (384. - offset.top) / 256, y);
        grid = next;
    }
    EXPECT_EQ(MWGui::LocalMapView::gridForViewport(full, 0, 0, 1024, 768, 256).left, -1);
    EXPECT_EQ(MWGui::LocalMapView::gridForViewport(full, double(full.right), double(full.bottom), 1024, 768, 256).right,
        full.right);
}

TEST(LocalMapViewTest, RemoteMarkersStayRepresentableWithoutLosingLogicalIdentity)
{
    const MyGUI::IntRect grid{999999999, 999999999, 1000000003, 1000000003};
    const auto remote = MWGui::LocalMapView::position(grid, std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max(), .25f, .75f, 2048);
    EXPECT_LT(remote.left, -1000000); EXPECT_LT(remote.top, -1000000);
    EXPECT_GT(remote.left, std::numeric_limits<int>::min() / 2);
    EXPECT_GT(remote.top, std::numeric_limits<int>::min() / 2);
    const auto nearby = MWGui::LocalMapView::position(grid, 1000000000, 1000000000, .25f, .75f, 2048);
    EXPECT_EQ(nearby.left, 2560); EXPECT_EQ(nearby.top, 7680);
}


TEST(QuickKeyResourcesTest, FramesPreserveAuthoredTexturesAndUseTheNativeFallback)
{
    TestingOpenMW::VFSTestFile file{"frame"};
    const auto vfs = TestingOpenMW::createTestVFS({
        {VFS::Path::NormalizedView("textures/omw_menu_icon_active.dds"), &file},
        {VFS::Path::NormalizedView("textures/menu_icon_select_magic.dds"), &file}});
    const std::string original = "textures\\menu_icon_select_magic.dds";
    const std::string absent = "textures\\menu_icon_select_magic_magic.dds";
    EXPECT_EQ(MWGui::QuickKeyResources::frame(original, ESM::GameProfile::Oblivion, *vfs), original);
    EXPECT_EQ(MWGui::QuickKeyResources::frame(absent, ESM::GameProfile::Oblivion, *vfs),
        "textures/omw_menu_icon_active.dds");
    EXPECT_EQ(MWGui::QuickKeyResources::frame(absent, ESM::GameProfile::Morrowind, *vfs), absent);
    EXPECT_TRUE(MWGui::QuickKeyResources::frame({}, ESM::GameProfile::Oblivion, *vfs).empty());
    VFS::Manager empty;
    EXPECT_EQ(MWGui::QuickKeyResources::frame(absent, ESM::GameProfile::Oblivion, empty), absent);
}

TEST(QuickKeyResourcesTest, InventoryIconsShareTheWidgetDefaultAndDdsCorrection)
{
    TestingOpenMW::VFSTestFile file{"icon"};
    const auto vfs = TestingOpenMW::createTestVFS({
        {VFS::Path::NormalizedView("icons/default icon.dds"), &file},
        {VFS::Path::NormalizedView("icons/test.dds"), &file}});
    EXPECT_EQ(MWGui::QuickKeyResources::inventoryIcon({}, *vfs).value(), "icons/default icon.dds");
    EXPECT_EQ(MWGui::QuickKeyResources::inventoryIcon(VFS::Path::NormalizedView("test.tga"), *vfs).value(),
        "icons/test.dds");
    EXPECT_EQ(MWGui::QuickKeyResources::inventoryIcon(VFS::Path::NormalizedView("missing.tga"), *vfs).value(),
        "icons/default icon.dds");
}
