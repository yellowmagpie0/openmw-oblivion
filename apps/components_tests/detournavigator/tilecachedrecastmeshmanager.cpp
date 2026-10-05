
#include <components/detournavigator/debug.hpp>
#include <components/detournavigator/settingsutils.hpp>
#include <components/detournavigator/tilecachedrecastmeshmanager.hpp>

#include <BulletCollision/CollisionShapes/btBoxShape.h>

#include <osg/io_utils>

#include <array>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
    using namespace testing;
    using namespace DetourNavigator;

    struct DetourNavigatorTileCachedRecastMeshManagerTest : Test
    {
        RecastSettings mSettings;
        const ObjectTransform mObjectTransform{ ESM::Position{ { 0, 0, 0 }, { 0, 0, 0 } }, 0.0f };
        const osg::ref_ptr<const Resource::BulletShape> mShape = new Resource::BulletShape;
        const osg::ref_ptr<const Resource::BulletShapeInstance> mInstance = new Resource::BulletShapeInstance(mShape);
        const ESM::RefId mWorldspace = ESM::RefId::stringRefId("worldspace");

        DetourNavigatorTileCachedRecastMeshManagerTest()
        {
            mSettings.mBorderSize = 16;
            mSettings.mCellSize = 0.2f;
            mSettings.mRecastScaleFactor = 0.017647058823529415f;
            mSettings.mTileSize = 64;
        }
    };

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, get_mesh_for_empty_should_return_nullptr)
    {
        TileCachedRecastMeshManager manager(mSettings);
        EXPECT_EQ(manager.getMesh(mWorldspace, TilePosition(0, 0)), nullptr);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, get_revision_for_empty_should_return_zero)
    {
        const TileCachedRecastMeshManager manager(mSettings);
        EXPECT_EQ(manager.getRevision(), 0);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, add_object_for_new_object_should_return_true)
    {
        TileCachedRecastMeshManager manager(mSettings);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        EXPECT_TRUE(manager.addObject(
            ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr));
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, add_object_for_existing_object_should_return_false)
    {
        TileCachedRecastMeshManager manager(mSettings);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        manager.addObject(ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);
        EXPECT_FALSE(manager.addObject(
            ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr));
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, add_object_should_add_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        manager.setWorldspace(mWorldspace, nullptr);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        ASSERT_TRUE(manager.addObject(
            ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr));
        for (int x = -1; x < 1; ++x)
            for (int y = -1; y < 1; ++y)
                ASSERT_NE(manager.getMesh(mWorldspace, TilePosition(x, y)), nullptr);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, add_object_should_return_add_changed_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        const TilesPositionsRange range{
            .mBegin = TilePosition(0, 0),
            .mEnd = TilePosition(1, 1),
        };
        manager.setRange(range, nullptr);
        manager.addObject(ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);
        EXPECT_THAT(manager.takeChangedTiles(nullptr), ElementsAre(std::pair(TilePosition(0, 0), ChangeType::add)));
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, update_object_for_changed_object_should_add_changed_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const btTransform transform(
            btMatrix3x3::getIdentity(), btVector3(getTileSize(mSettings) / mSettings.mRecastScaleFactor, 0, 0));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        const TilesPositionsRange range{
            .mBegin = TilePosition(-1, -1),
            .mEnd = TilePosition(2, 2),
        };
        manager.setRange(range, nullptr);
        manager.addObject(ObjectId(&boxShape), shape, transform, AreaType::AreaType_ground, nullptr);
        manager.takeChangedTiles(nullptr);
        EXPECT_TRUE(
            manager.updateObject(ObjectId(&boxShape), btTransform::getIdentity(), AreaType::AreaType_ground, nullptr));
        EXPECT_THAT(manager.takeChangedTiles(nullptr),
            ElementsAre(std::pair(TilePosition(-1, -1), ChangeType::add),
                std::pair(TilePosition(-1, 0), ChangeType::add), std::pair(TilePosition(0, -1), ChangeType::update),
                std::pair(TilePosition(0, 0), ChangeType::update), std::pair(TilePosition(1, -1), ChangeType::remove),
                std::pair(TilePosition(1, 0), ChangeType::remove)));
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest,
        update_object_for_not_changed_object_should_not_add_changed_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        manager.addObject(ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);
        manager.takeChangedTiles(nullptr);
        EXPECT_FALSE(
            manager.updateObject(ObjectId(&boxShape), btTransform::getIdentity(), AreaType::AreaType_ground, nullptr));
        EXPECT_THAT(manager.takeChangedTiles(nullptr), IsEmpty());
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, remove_object_should_return_add_changed_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        const TilesPositionsRange range{
            .mBegin = TilePosition(0, 0),
            .mEnd = TilePosition(1, 1),
        };
        manager.setRange(range, nullptr);
        manager.addObject(ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);
        manager.takeChangedTiles(nullptr);
        manager.removeObject(ObjectId(&boxShape), nullptr);
        EXPECT_THAT(manager.takeChangedTiles(nullptr), ElementsAre(std::pair(TilePosition(0, 0), ChangeType::remove)));
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest,
        get_mesh_after_add_object_should_return_recast_mesh_for_each_used_tile)
    {
        TileCachedRecastMeshManager manager(mSettings);
        manager.setWorldspace(mWorldspace, nullptr);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        manager.addObject(ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(-1, -1)), nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(-1, 0)), nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(0, -1)), nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(0, 0)), nullptr);
    }

    TEST_F(
        DetourNavigatorTileCachedRecastMeshManagerTest, get_mesh_after_add_object_should_return_nullptr_for_unused_tile)
    {
        TileCachedRecastMeshManager manager(mSettings);
        manager.setWorldspace(mWorldspace, nullptr);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        manager.addObject(ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);
        EXPECT_EQ(manager.getMesh(mWorldspace, TilePosition(1, 0)), nullptr);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest,
        get_mesh_for_moved_object_should_return_recast_mesh_for_each_used_tile)
    {
        TileCachedRecastMeshManager manager(mSettings);
        const TilesPositionsRange range{
            .mBegin = TilePosition(-1, -1),
            .mEnd = TilePosition(2, 2),
        };
        manager.setRange(range, nullptr);
        manager.setWorldspace(mWorldspace, nullptr);

        const btBoxShape boxShape(btVector3(20, 20, 100));
        const btTransform transform(
            btMatrix3x3::getIdentity(), btVector3(getTileSize(mSettings) / mSettings.mRecastScaleFactor, 0, 0));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);

        manager.addObject(ObjectId(&boxShape), shape, transform, AreaType::AreaType_ground, nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(0, -1)), nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(0, 0)), nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(1, 0)), nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(1, -1)), nullptr);

        manager.updateObject(ObjectId(&boxShape), btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(-1, -1)), nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(-1, 0)), nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(0, -1)), nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(0, 0)), nullptr);
    }

    TEST_F(
        DetourNavigatorTileCachedRecastMeshManagerTest, get_mesh_for_moved_object_should_return_nullptr_for_unused_tile)
    {
        TileCachedRecastMeshManager manager(mSettings);
        manager.setWorldspace(mWorldspace, nullptr);

        const btBoxShape boxShape(btVector3(20, 20, 100));
        const btTransform transform(
            btMatrix3x3::getIdentity(), btVector3(getTileSize(mSettings) / mSettings.mRecastScaleFactor, 0, 0));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);

        manager.addObject(ObjectId(&boxShape), shape, transform, AreaType::AreaType_ground, nullptr);
        EXPECT_EQ(manager.getMesh(mWorldspace, TilePosition(-1, -1)), nullptr);
        EXPECT_EQ(manager.getMesh(mWorldspace, TilePosition(-1, 0)), nullptr);

        manager.updateObject(ObjectId(&boxShape), btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);
        EXPECT_EQ(manager.getMesh(mWorldspace, TilePosition(1, 0)), nullptr);
        EXPECT_EQ(manager.getMesh(mWorldspace, TilePosition(1, -1)), nullptr);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest,
        get_mesh_for_removed_object_should_return_nullptr_for_all_previously_used_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        manager.setWorldspace(mWorldspace, nullptr);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        manager.addObject(ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);
        manager.removeObject(ObjectId(&boxShape), nullptr);
        EXPECT_EQ(manager.getMesh(mWorldspace, TilePosition(-1, -1)), nullptr);
        EXPECT_EQ(manager.getMesh(mWorldspace, TilePosition(-1, 0)), nullptr);
        EXPECT_EQ(manager.getMesh(mWorldspace, TilePosition(0, -1)), nullptr);
        EXPECT_EQ(manager.getMesh(mWorldspace, TilePosition(0, 0)), nullptr);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest,
        get_mesh_for_not_changed_object_after_update_should_return_recast_mesh_for_same_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        manager.setWorldspace(mWorldspace, nullptr);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);

        manager.addObject(ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(-1, -1)), nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(-1, 0)), nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(0, -1)), nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(0, 0)), nullptr);

        manager.updateObject(ObjectId(&boxShape), btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(-1, -1)), nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(-1, 0)), nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(0, -1)), nullptr);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(0, 0)), nullptr);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest,
        get_revision_after_add_object_new_should_return_incremented_value)
    {
        TileCachedRecastMeshManager manager(mSettings);
        const auto initialRevision = manager.getRevision();
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        manager.addObject(ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);
        EXPECT_EQ(manager.getRevision(), initialRevision + 1);
    }

    TEST_F(
        DetourNavigatorTileCachedRecastMeshManagerTest, get_revision_after_add_object_existing_should_return_same_value)
    {
        TileCachedRecastMeshManager manager(mSettings);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        manager.addObject(ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);
        const auto beforeAddRevision = manager.getRevision();
        EXPECT_FALSE(manager.addObject(
            ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr));
        EXPECT_EQ(manager.getRevision(), beforeAddRevision);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest,
        get_revision_after_update_moved_object_should_return_incremented_value)
    {
        TileCachedRecastMeshManager manager(mSettings);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const btTransform transform(
            btMatrix3x3::getIdentity(), btVector3(getTileSize(mSettings) / mSettings.mRecastScaleFactor, 0, 0));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        manager.addObject(ObjectId(&boxShape), shape, transform, AreaType::AreaType_ground, nullptr);
        const auto beforeUpdateRevision = manager.getRevision();
        manager.updateObject(ObjectId(&boxShape), btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);
        EXPECT_EQ(manager.getRevision(), beforeUpdateRevision + 1);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest,
        get_revision_after_update_not_changed_object_should_return_same_value)
    {
        TileCachedRecastMeshManager manager(mSettings);
        manager.setWorldspace(mWorldspace, nullptr);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        manager.addObject(ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);
        const auto beforeUpdateRevision = manager.getRevision();
        manager.updateObject(ObjectId(&boxShape), btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);
        EXPECT_EQ(manager.getRevision(), beforeUpdateRevision);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest,
        get_revision_after_remove_existing_object_should_return_incremented_value)
    {
        TileCachedRecastMeshManager manager(mSettings);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        manager.addObject(ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);
        const auto beforeRemoveRevision = manager.getRevision();
        manager.removeObject(ObjectId(&boxShape), nullptr);
        EXPECT_EQ(manager.getRevision(), beforeRemoveRevision + 1);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest,
        get_revision_after_remove_absent_object_should_return_same_value)
    {
        TileCachedRecastMeshManager manager(mSettings);
        const auto beforeRemoveRevision = manager.getRevision();
        manager.removeObject(ObjectId(&manager), nullptr);
        EXPECT_EQ(manager.getRevision(), beforeRemoveRevision);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, add_water_for_new_water_should_add_changed_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        const osg::Vec2i cellPosition(0, 0);
        const int cellSize = 8192;
        manager.addWater(cellPosition, cellSize, 0.0f, nullptr);
        const auto changedTiles = manager.takeChangedTiles(nullptr);
        EXPECT_EQ(changedTiles.begin()->first, TilePosition(-1, -1));
        EXPECT_EQ(changedTiles.rbegin()->first, TilePosition(11, 11));
        for (const auto& [k, v] : changedTiles)
            EXPECT_EQ(v, ChangeType::add) << k;
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, add_water_for_not_max_int_should_add_new_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        manager.setWorldspace(mWorldspace, nullptr);
        const osg::Vec2i cellPosition(0, 0);
        const int cellSize = 8192;
        manager.addWater(cellPosition, cellSize, 0.0f, nullptr);
        for (int x = -1; x < 12; ++x)
            for (int y = -1; y < 12; ++y)
                ASSERT_NE(manager.getMesh(mWorldspace, TilePosition(x, y)), nullptr);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, add_water_for_max_int_should_not_add_new_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        manager.setWorldspace(mWorldspace, nullptr);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        ASSERT_TRUE(manager.addObject(
            ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr));
        const osg::Vec2i cellPosition(0, 0);
        const int cellSize = std::numeric_limits<int>::max();
        manager.addWater(cellPosition, cellSize, 0.0f, nullptr);
        for (int x = -6; x < 6; ++x)
            for (int y = -6; y < 6; ++y)
                ASSERT_EQ(manager.getMesh(mWorldspace, TilePosition(x, y)) != nullptr,
                    -1 <= x && x <= 0 && -1 <= y && y <= 0);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, remove_water_for_absent_cell_should_not_add_changed_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        manager.removeWater(osg::Vec2i(0, 0), nullptr);
        EXPECT_THAT(manager.takeChangedTiles(nullptr), ElementsAre());
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, remove_water_for_existing_cell_should_add_changed_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        const osg::Vec2i cellPosition(0, 0);
        const int cellSize = 8192;
        manager.addWater(cellPosition, cellSize, 0.0f, nullptr);
        manager.takeChangedTiles(nullptr);
        manager.removeWater(cellPosition, nullptr);
        const auto changedTiles = manager.takeChangedTiles(nullptr);
        EXPECT_EQ(changedTiles.begin()->first, TilePosition(-1, -1));
        EXPECT_EQ(changedTiles.rbegin()->first, TilePosition(11, 11));
        for (const auto& [k, v] : changedTiles)
            EXPECT_EQ(v, ChangeType::remove) << k;
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, remove_water_for_existing_cell_should_remove_empty_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        manager.setWorldspace(mWorldspace, nullptr);
        const osg::Vec2i cellPosition(0, 0);
        const int cellSize = 8192;
        manager.addWater(cellPosition, cellSize, 0.0f, nullptr);
        manager.removeWater(cellPosition, nullptr);
        for (int x = -6; x < 6; ++x)
            for (int y = -6; y < 6; ++y)
                ASSERT_EQ(manager.getMesh(mWorldspace, TilePosition(x, y)), nullptr);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, remove_water_for_existing_cell_should_leave_not_empty_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        manager.setWorldspace(mWorldspace, nullptr);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        ASSERT_TRUE(manager.addObject(
            ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr));
        const osg::Vec2i cellPosition(0, 0);
        const int cellSize = 8192;
        manager.addWater(cellPosition, cellSize, 0.0f, nullptr);
        manager.removeWater(cellPosition, nullptr);
        for (int x = -6; x < 6; ++x)
            for (int y = -6; y < 6; ++y)
                ASSERT_EQ(manager.getMesh(mWorldspace, TilePosition(x, y)) != nullptr,
                    -1 <= x && x <= 0 && -1 <= y && y <= 0);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, remove_object_should_not_remove_tile_with_water)
    {
        TileCachedRecastMeshManager manager(mSettings);
        manager.setWorldspace(mWorldspace, nullptr);
        const osg::Vec2i cellPosition(0, 0);
        const int cellSize = 8192;
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        ASSERT_TRUE(manager.addObject(
            ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr));
        manager.addWater(cellPosition, cellSize, 0.0f, nullptr);
        manager.removeObject(ObjectId(&boxShape), nullptr);
        for (int x = -1; x < 12; ++x)
            for (int y = -1; y < 12; ++y)
                ASSERT_NE(manager.getMesh(mWorldspace, TilePosition(x, y)), nullptr);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, set_new_worldspace_should_remove_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        manager.setWorldspace(mWorldspace, nullptr);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(nullptr, boxShape, mObjectTransform);
        ASSERT_TRUE(manager.addObject(
            ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr));
        const ESM::RefId otherWorldspace(ESM::FormId::fromUint32(0x1));
        manager.setWorldspace(ESM::FormId::fromUint32(0x1), nullptr);
        for (int x = -1; x < 1; ++x)
            for (int y = -1; y < 1; ++y)
                ASSERT_EQ(manager.getMesh(otherWorldspace, TilePosition(x, y)), nullptr);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, set_range_should_add_changed_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        const btBoxShape boxShape(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        const TilesPositionsRange range1{
            .mBegin = TilePosition(0, 0),
            .mEnd = TilePosition(1, 1),
        };
        manager.setRange(range1, nullptr);
        manager.addObject(ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);
        const TilesPositionsRange range2{
            .mBegin = TilePosition(-1, -1),
            .mEnd = TilePosition(0, 0),
        };
        manager.takeChangedTiles(nullptr);
        manager.setRange(range2, nullptr);
        EXPECT_THAT(manager.takeChangedTiles(nullptr),
            ElementsAre(
                std::pair(TilePosition(-1, -1), ChangeType::add), std::pair(TilePosition(0, 0), ChangeType::remove)));
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, set_range_should_remove_cached_recast_meshes_outside_range)
    {
        TileCachedRecastMeshManager manager(mSettings);

        manager.setWorldspace(mWorldspace, nullptr);

        const btBoxShape boxShape(btVector3(100, 100, 20));
        const CollisionShape shape(mInstance, boxShape, mObjectTransform);
        const TilesPositionsRange range1{
            .mBegin = TilePosition(0, 0),
            .mEnd = TilePosition(1, 1),
        };
        manager.setRange(range1, nullptr);
        manager.addObject(ObjectId(&boxShape), shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr);

        const TilePosition tilePosition(0, 0);

        ASSERT_EQ(manager.getCachedMesh(mWorldspace, tilePosition), nullptr);
        ASSERT_NE(manager.getMesh(mWorldspace, tilePosition), nullptr);
        ASSERT_NE(manager.getCachedMesh(mWorldspace, tilePosition), nullptr);

        const TilesPositionsRange range2{
            .mBegin = TilePosition(-1, -1),
            .mEnd = TilePosition(0, 0),
        };
        manager.takeChangedTiles(nullptr);
        manager.setRange(range2, nullptr);

        ASSERT_EQ(manager.getCachedMesh(mWorldspace, tilePosition), nullptr);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, add_heightfield_plane_should_add_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        manager.setWorldspace(mWorldspace, nullptr);
        const osg::Vec2i cellPosition(1, 2);
        const int cellSize = 32;
        const HeightfieldPlane plane{
            .mHeight = 1000,
        };
        manager.addHeightfield(cellPosition, cellSize, plane, nullptr);
        EXPECT_THAT(manager.takeChangedTiles(nullptr),
            ElementsAre( //
                std::pair(TilePosition(-1, -1), ChangeType::add), //
                std::pair(TilePosition(-1, 0), ChangeType::add), //
                std::pair(TilePosition(0, -1), ChangeType::add), //
                std::pair(TilePosition(0, 0), ChangeType::add) //
                ));
        for (int x = -1; x < 1; ++x)
            for (int y = -1; y < 1; ++y)
                EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(x, y)), nullptr) << x << " " << y;
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, add_heightfield_surface_should_add_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        manager.setWorldspace(mWorldspace, nullptr);
        const osg::Vec2i cellPosition(1, 2);
        const int cellSize = 32;
        const std::array<float, 25> heights{ {
            1000, 1100, 1200, 1300, 1400, // 0
            1100, 1200, 1300, 1400, 1600, // 1
            1200, 1300, 1400, 1500, 1500, // 2
            1400, 1500, 1600, 1700, 1800, // 3
            1600, 1700, 1800, 1900, 2000, // 4
        } };
        const HeightfieldSurface surface{
            .mHeights = heights.data(),
            .mSize = 5,
            .mMinHeight = 1000,
            .mMaxHeight = 2000,
        };
        manager.addHeightfield(cellPosition, cellSize, surface, nullptr);
        EXPECT_THAT(manager.takeChangedTiles(nullptr),
            ElementsAre( //
                std::pair(TilePosition(-1, -1), ChangeType::add), //
                std::pair(TilePosition(-1, 0), ChangeType::add), //
                std::pair(TilePosition(0, -1), ChangeType::add), //
                std::pair(TilePosition(0, 0), ChangeType::add) //
                ));
        for (int x = -1; x < 1; ++x)
            for (int y = -1; y < 1; ++y)
                EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(x, y)), nullptr) << x << " " << y;
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, add_water_should_add_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings);
        manager.setWorldspace(mWorldspace, nullptr);
        const osg::Vec2i cellPosition(1, 2);
        const int cellSize = 32;
        const float level = 1000;
        manager.addWater(cellPosition, cellSize, level, nullptr);
        EXPECT_THAT(manager.takeChangedTiles(nullptr),
            ElementsAre( //
                std::pair(TilePosition(-1, -1), ChangeType::add), //
                std::pair(TilePosition(-1, 0), ChangeType::add), //
                std::pair(TilePosition(0, -1), ChangeType::add), //
                std::pair(TilePosition(0, 0), ChangeType::add) //
                ));
        for (int x = -1; x < 1; ++x)
            for (int y = -1; y < 1; ++y)
                EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(x, y)), nullptr) << x << " " << y;
    }
}

namespace
{
    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, prepared_removal_cancellation_preserves_live_mesh)
    {
        TileCachedRecastMeshManager manager(mSettings); manager.setWorldspace(mWorldspace, nullptr);
        manager.setRange({TilePosition(0, 0), TilePosition(1, 1)}, nullptr);
        const btBoxShape box(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, box, mObjectTransform);
        const ObjectId id(&box);
        ASSERT_TRUE(manager.addObject(id, shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr));
        manager.takeChangedTiles(nullptr);
        const auto revision = manager.getRevision();
        { const std::array ids{id}; auto prepared = manager.prepareObjectRemoval(ids); EXPECT_TRUE(prepared->isValid()); }
        EXPECT_EQ(manager.getRevision(), revision);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(0, 0)), nullptr);
        EXPECT_THAT(manager.takeChangedTiles(nullptr), IsEmpty());
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, prepared_batch_removes_unique_existing_ids_once)
    {
        TileCachedRecastMeshManager manager(mSettings); manager.setWorldspace(mWorldspace, nullptr);
        manager.setRange({TilePosition(0, 0), TilePosition(1, 1)}, nullptr);
        const btBoxShape first(btVector3(20, 20, 100)), second(btVector3(10, 10, 100));
        const CollisionShape a(mInstance, first, mObjectTransform), b(mInstance, second, mObjectTransform);
        ASSERT_TRUE(manager.addObject(ObjectId(&first), a, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr));
        ASSERT_TRUE(manager.addObject(ObjectId(&second), b, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr));
        manager.takeChangedTiles(nullptr);
        const auto revision = manager.getRevision();
        {
            const std::array ids{ObjectId(&first), ObjectId(&first), ObjectId(&second), ObjectId(&manager)};
            auto prepared = manager.prepareObjectRemoval(ids);
            ASSERT_TRUE(prepared->commit()); EXPECT_FALSE(prepared->commit()); EXPECT_FALSE(prepared->isValid());
        }
        EXPECT_EQ(manager.getRevision(), revision + 2);
        EXPECT_EQ(manager.getMesh(mWorldspace, TilePosition(0, 0)), nullptr);
        EXPECT_THAT(manager.takeChangedTiles(nullptr), ElementsAre(std::pair(TilePosition(0, 0), ChangeType::remove)));
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, prepared_removal_preserves_other_object_identity_and_tiles)
    {
        TileCachedRecastMeshManager manager(mSettings); manager.setWorldspace(mWorldspace, nullptr);
        manager.setRange({TilePosition(0, 0), TilePosition(2, 2)}, nullptr);
        const btBoxShape first(btVector3(20, 20, 100)), second(btVector3(10, 10, 100));
        const CollisionShape a(mInstance, first, mObjectTransform), b(mInstance, second, mObjectTransform);
        ASSERT_TRUE(manager.addObject(ObjectId(&first), a, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr));
        ASSERT_TRUE(manager.addObject(ObjectId(&second), b, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr));
        manager.takeChangedTiles(nullptr);
        manager.addChangedTile(TilePosition(1, 1), ChangeType::add);
        {
            const std::array ids{ObjectId(&first)};
            const std::array changes{std::pair{TilePosition(1, 1), ChangeType::update},
                std::pair{TilePosition(0, 0), ChangeType::add}};
            auto prepared = manager.prepareObjectRemoval(ids, changes);
            ASSERT_TRUE(prepared->commit());
        }
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(0, 0)), nullptr);
        EXPECT_THAT(manager.takeChangedTiles(nullptr),
            ElementsAre(std::pair(TilePosition(0, 0), ChangeType::remove),
                std::pair(TilePosition(1, 1), ChangeType::add)));
        // An unaffected binding remains live and still accepts its ordinary removal.
        manager.removeObject(ObjectId(&second), nullptr);
        EXPECT_EQ(manager.getMesh(mWorldspace, TilePosition(0, 0)), nullptr);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, prepared_removal_rejects_changed_notice_map_without_erasing_object)
    {
        TileCachedRecastMeshManager manager(mSettings); manager.setWorldspace(mWorldspace, nullptr);
        manager.setRange({TilePosition(0, 0), TilePosition(2, 2)}, nullptr);
        const btBoxShape box(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, box, mObjectTransform); const ObjectId id(&box);
        ASSERT_TRUE(manager.addObject(id, shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr));
        manager.takeChangedTiles(nullptr);
        const auto revision = manager.getRevision();
        {
            const std::array ids{id}; auto prepared = manager.prepareObjectRemoval(ids);
            manager.addChangedTile(TilePosition(1, 1), ChangeType::update);
            EXPECT_FALSE(prepared->isValid()); EXPECT_FALSE(prepared->commit());
        }
        EXPECT_EQ(manager.getRevision(), revision);
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(0, 0)), nullptr);
        EXPECT_THAT(manager.takeChangedTiles(nullptr), ElementsAre(std::pair(TilePosition(1, 1), ChangeType::update)));
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, prepared_removal_rejects_guarded_object_updates)
    {
        TileCachedRecastMeshManager manager(mSettings); manager.setWorldspace(mWorldspace, nullptr);
        manager.setRange({TilePosition(0, 0), TilePosition(2, 2)}, nullptr);
        const btBoxShape box(btVector3(20, 20, 100));
        const CollisionShape shape(mInstance, box, mObjectTransform); const ObjectId id(&box);
        ASSERT_TRUE(manager.addObject(id, shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr));
        {
            auto guard = manager.makeUpdateGuard(); const std::array ids{id};
            auto prepared = manager.prepareObjectRemoval(ids, {}, guard.get());
            btTransform moved = btTransform::getIdentity(); moved.setOrigin(btVector3(10, 10, 0));
            ASSERT_TRUE(manager.updateObject(id, moved, AreaType::AreaType_ground, guard.get()));
            EXPECT_FALSE(prepared->isValid()); EXPECT_FALSE(prepared->commit());
        }
        EXPECT_NE(manager.getMesh(mWorldspace, TilePosition(0, 0)), nullptr);
    }

    TEST_F(DetourNavigatorTileCachedRecastMeshManagerTest, prepared_external_notices_advance_revision_without_objects)
    {
        TileCachedRecastMeshManager manager(mSettings);
        const auto revision = manager.getRevision();
        {
            const std::array changes{std::pair{TilePosition(1, 1), ChangeType::update}};
            auto prepared = manager.prepareObjectRemoval({}, changes);
            ASSERT_TRUE(prepared->commit());
        }
        EXPECT_EQ(manager.getRevision(), revision + 1);
        EXPECT_THAT(manager.takeChangedTiles(nullptr), ElementsAre(std::pair(TilePosition(1, 1), ChangeType::update)));
        const auto unchanged = manager.getRevision();
        manager.removeObject(ObjectId(&manager), nullptr);
        EXPECT_EQ(manager.getRevision(), unchanged);
        EXPECT_THAT(manager.takeChangedTiles(nullptr), IsEmpty());
    }
}
