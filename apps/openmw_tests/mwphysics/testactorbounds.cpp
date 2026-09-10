#include <apps/openmw/mwphysics/actor.hpp>
#include <apps/openmw/mwphysics/mtphysics.hpp>
#include <apps/openmw/mwbase/environment.hpp>
#include <apps/openmw/mwmechanics/creaturestats.hpp>
#include <apps/openmw/mwclass/static.hpp>
#include <apps/openmw/mwworld/esmstore.hpp>
#include <apps/openmw/mwworld/livecellref.hpp>

#include <components/esm3/loadstat.hpp>
#include <components/resource/bulletshape.hpp>

#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>

#include <gtest/gtest.h>

namespace
{
    // Only the reference-class services required by the real physics Actor.
    // Local to this reference: never replace the process-wide class registry.
    class BoundsActorClass final : public MWWorld::Class
    {
        bool mNpc;
        mutable MWMechanics::CreatureStats mStats;
    public:
        explicit BoundsActorClass(bool npc) : Class(ESM::REC_STAT), mNpc(npc) {}
        bool isNpc() const override { return mNpc; }
        std::string_view getName(const MWWorld::ConstPtr&) const override { return "bounds fixture"; }
        MWMechanics::CreatureStats& getCreatureStats(const MWWorld::Ptr&) const override { return mStats; }
    };

    TEST(ActorBoundsTest, FallbackIsOptInAndPreservesLegacyNpcAndCreatureBounds)
    {
        MWClass::Static::registerSelf();
        MWBase::Environment environment;
        MWWorld::ESMStore store;
        environment.setESMStore(store);
        btDefaultCollisionConfiguration configuration;
        btCollisionDispatcher dispatcher(&configuration);
        btDbvtBroadphase broadphase;
        btCollisionWorld world(&dispatcher, &broadphase, &configuration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        ESM::Static base;
        base.blank();
        ESM::CellRef reference;
        reference.blank();
        MWWorld::LiveCellRef<ESM::Static> live(reference, &base);
        const BoundsActorClass npc(true), creature(false);
        MWWorld::Ptr ptr(&live);
        const osg::Vec3f fallback(16, 16, 64);
        osg::ref_ptr<Resource::BulletShape> source = new Resource::BulletShape;
        auto check = [&](const BoundsActorClass& type, const osg::Vec3f& supplied, const osg::Vec3f& expected) {
            live.mClass = &type;
            {
                MWPhysics::Actor actor(ptr, source, &scheduler, false,
                    DetourNavigator::CollisionShapeType::Aabb, supplied);
                EXPECT_EQ(actor.getOriginalHalfExtents(), expected);
                EXPECT_EQ(actor.getHalfExtents(), expected);
                EXPECT_EQ(world.getNumCollisionObjects(), 1);
            }
            EXPECT_EQ(world.getNumCollisionObjects(), 0);
        };
        check(npc, {}, {});
        check(npc, fallback, fallback); // Includes Oblivion's projected TES3 player.
        check(creature, {}, {}); // Legacy malformed creature: report, don't invent bounds.
        check(creature, fallback, fallback);
        source->mCollisionShape.reset(new btBoxShape(btVector3(4, 5, 6)));
        check(npc, {}, {}); // TES3 NPCs must not derive bounds from this shape.
        check(creature, {}, osg::Vec3f(4, 5, 6));
        check(npc, fallback, osg::Vec3f(4, 5, 6));
        source->mCollisionBox.mExtents = osg::Vec3f(7, 8, 9);
        check(npc, {}, osg::Vec3f(7, 8, 9));
        check(npc, fallback, osg::Vec3f(7, 8, 9));
        check(creature, fallback, osg::Vec3f(7, 8, 9));
    }
}
