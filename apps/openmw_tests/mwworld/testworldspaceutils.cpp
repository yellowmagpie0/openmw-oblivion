#include <gtest/gtest.h>

#include <components/esm4/loadwrld.hpp>

#include "apps/openmw/mwworld/store.hpp"
#include "apps/openmw/mwworld/worldspaceutils.hpp"

namespace
{
    ESM4::World makeWorld(std::uint32_t id, std::uint32_t parent = 0, std::uint16_t flags = 0)
    {
        ESM4::World world;
        world.mId = ESM::FormId::fromUint32(id);
        world.mParent = ESM::FormId::fromUint32(parent);
        world.mParentUseFlags = flags;
        return world;
    }
}

TEST(MWWorldspaceUtilsTest, resolvesCompletePerFeatureInheritanceChain)
{
    MWWorld::Store<ESM4::World> worlds;
    worlds.insertStatic(makeWorld(1));
    worlds.insertStatic(makeWorld(2, 1, ESM4::World::UseFlag_Land | ESM4::World::UseFlag_Water));
    worlds.insertStatic(makeWorld(3, 2, ESM4::World::UseFlag_Land));

    const ESM::RefId child = ESM::RefId(ESM::FormId::fromUint32(3));
    EXPECT_EQ(MWWorld::resolveWorldspaceInheritance(worlds, child, ESM4::World::UseFlag_Land),
        ESM::RefId(ESM::FormId::fromUint32(1)));
    EXPECT_EQ(MWWorld::resolveWorldspaceInheritance(worlds, child, ESM4::World::UseFlag_Water), child);
}

TEST(MWWorldspaceUtilsTest, stopsSafelyAtMissingParentAndCycles)
{
    MWWorld::Store<ESM4::World> worlds;
    worlds.insertStatic(makeWorld(4, 5, ESM4::World::UseFlag_LOD));
    worlds.insertStatic(makeWorld(5, 4, ESM4::World::UseFlag_LOD));
    worlds.insertStatic(makeWorld(6, 7, ESM4::World::UseFlag_Map));

    EXPECT_EQ(MWWorld::resolveWorldspaceInheritance(
                  worlds, ESM::RefId(ESM::FormId::fromUint32(4)), ESM4::World::UseFlag_LOD),
        ESM::RefId(ESM::FormId::fromUint32(4)));
    EXPECT_EQ(MWWorld::resolveWorldspaceInheritance(
                  worlds, ESM::RefId(ESM::FormId::fromUint32(6)), ESM4::World::UseFlag_Map),
        ESM::RefId(ESM::FormId::fromUint32(7)));
}
