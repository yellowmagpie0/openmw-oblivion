// Standalone allocation-failure probe. Build in an isolated process against the
// actual components library; never link this global allocator into game/tests.
#include <components/detournavigator/tilecachedrecastmeshmanager.hpp>
#include <components/detournavigator/settings.hpp>
#include <components/resource/bulletshape.hpp>
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <cstdlib>
#include <iostream>
#include <new>

namespace
{
    thread_local int allocationLimit = -1;
    struct FailAllocation
    {
        explicit FailAllocation(int limit) { allocationLimit = limit; }
        ~FailAllocation() { allocationLimit = -1; }
    };
}
void* operator new(std::size_t size)
{
    if (allocationLimit == 0) throw std::bad_alloc();
    if (allocationLimit > 0) --allocationLimit;
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main()
{
    using namespace DetourNavigator;
    int failures = 0, successes = 0;
    for (int limit = 0; limit < 65; ++limit)
    {
        RecastSettings settings;
        settings.mBorderSize = 16; settings.mCellSize = 0.2f;
        settings.mRecastScaleFactor = 0.017647058823529415f; settings.mTileSize = 64;
        TileCachedRecastMeshManager manager(settings);
        const auto worldspace = ESM::RefId::stringRefId("allocation-probe");
        manager.setWorldspace(worldspace, nullptr);
        manager.setRange({TilePosition(0, 0), TilePosition(1, 1)}, nullptr);
        const osg::ref_ptr<Resource::BulletShape> source = new Resource::BulletShape;
        const osg::ref_ptr<Resource::BulletShapeInstance> instance = new Resource::BulletShapeInstance(source);
        const ObjectTransform placement{ESM::Position{{0, 0, 0}, {0, 0, 0}}, 0};
        const btBoxShape box(btVector3(20, 20, 100));
        const CollisionShape shape(instance, box, placement);
        const ObjectId id(&box);
        if (!manager.addObject(id, shape, btTransform::getIdentity(), AreaType::AreaType_ground, nullptr)
            || !manager.getMesh(worldspace, TilePosition(0, 0)))
        {
            std::cout << "{\"error\":\"invalid_setup\"}\n"; return 99;
        }
        manager.takeChangedTiles(nullptr);
        const auto revision = manager.getRevision();
        bool rejected = false;
        try
        {
            FailAllocation failure(limit);
            manager.removeObject(id, nullptr);
        }
        catch (const std::bad_alloc&) { rejected = true; }
        const auto changed = manager.takeChangedTiles(nullptr);
        const bool present = bool(manager.getMesh(worldspace, TilePosition(0, 0)));
        if (rejected)
        {
            ++failures;
            if (manager.getRevision() != revision || !present || !changed.empty())
            {
                std::cout << "{\"error\":\"partial_removal\",\"allocation_limit\":" << limit
                          << ",\"old_revision\":" << revision << ",\"revision\":" << manager.getRevision()
                          << ",\"object_present\":" << (present ? "true" : "false") << "}\n";
                return 17;
            }
        }
        else
        {
            ++successes;
            if (manager.getRevision() != revision + 1 || present || changed.size() != 1
                || changed.begin()->first != TilePosition(0, 0) || changed.begin()->second != ChangeType::remove)
            {
                std::cout << "{\"error\":\"incorrect_success\",\"allocation_limit\":" << limit << "}\n";
                return 18;
            }
        }
    }
    if (!failures || !successes) return 19;
    std::cout << "{\"passed\":true,\"cases\":65,\"allocation_failures\":" << failures
              << ",\"successful_removals\":" << successes << "}\n";
    return 0;
}
