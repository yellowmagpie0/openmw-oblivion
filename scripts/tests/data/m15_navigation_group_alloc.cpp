// Reuse the isolated allocator; this file supplies the group-removal exercise.
#define main m15SingleNavigationRemovalMain
#include "m15_navigation_removal_alloc.cpp"
#undef main
#include <apps/components_tests/detournavigator/settings.hpp>
#include <components/detournavigator/navigatorimpl.hpp>
#include <components/detournavigator/navmeshdb.hpp>
#include <components/detournavigator/stats.hpp>

int main()
{
    using namespace DetourNavigator;
    int failures = 0, successes = 0;
    for (int limit = 0; limit < 65; ++limit)
    {
        auto settings = Tests::makeSettings();
        NavigatorImpl navigator(settings, nullptr);
        const osg::ref_ptr<Resource::BulletShape> shape = new Resource::BulletShape;
        shape->mCollisionShape.reset(new btBoxShape(btVector3(20, 20, 100)));
        shape->mAvoidCollisionShape.reset(new btBoxShape(btVector3(10, 10, 100)));
        const osg::ref_ptr<Resource::BulletShapeInstance> instance = new Resource::BulletShapeInstance(shape);
        const ObjectTransform placement{ESM::Position{{0, 0, 0}, {0, 0, 0}}, 0};
        const ObjectId id(instance->mCollisionShape.get());
        navigator.addObject(id, DoorShapes(instance, placement, osg::Vec3f(0, 0, 0), osg::Vec3f(500, 500, 0)),
            btTransform::getIdentity(), nullptr);
        if (navigator.getStats().mRecast.mObjects != 2)
        {
            std::cout << "{\"error\":\"invalid_setup\"}\n"; return 99;
        }
        bool rejected = false;
        try
        {
            FailAllocation failure(limit);
            navigator.removeObject(id, nullptr);
        }
        catch (const std::bad_alloc&) { rejected = true; }
        const auto objects = navigator.getStats().mRecast.mObjects;
        if (rejected)
        {
            ++failures;
            if (objects != 2)
            {
                std::cout << "{\"error\":\"partial_removal\",\"allocation_limit\":" << limit
                          << ",\"objects_before\":2,\"objects_after\":" << objects << "}\n";
                return 17;
            }
        }
        else
        {
            ++successes;
            if (objects != 0)
            {
                std::cout << "{\"error\":\"incorrect_success\",\"allocation_limit\":" << limit << "}\n";
                return 18;
            }
        }
    }
    if (!failures || !successes) return 19;
    std::cout << "{\"passed\":true,\"cases\":65,\"allocation_failures\":" << failures
              << ",\"successful_removals\":" << successes << "}\n";
}
