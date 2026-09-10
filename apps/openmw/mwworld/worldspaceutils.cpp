#include "worldspaceutils.hpp"

#include <unordered_set>

#include <components/esm4/loadwrld.hpp>

#include "store.hpp"

namespace MWWorld
{
    bool hasWorldspaceLandscape(const ESM4::World& worldspace, ESM::GameProfile gameProfile)
    {
        // WRLD flag 0x10 means "No LOD Water" in TES4 and "No Landscape" in later formats.
        return gameProfile == ESM::GameProfile::Oblivion
            || !(worldspace.mWorldFlags & ESM4::World::WLD_NoLandscpe);
    }

    ESM::RefId resolveWorldspaceInheritance(
        const Store<ESM4::World>& worlds, ESM::RefId worldspace, std::uint16_t useFlag)
    {
        std::unordered_set<ESM::RefId> visited;
        while (!worldspace.empty() && visited.insert(worldspace).second)
        {
            const ESM4::World* world = worlds.search(worldspace);
            if (world == nullptr || world->mParent.isZeroOrUnset() || !(world->mParentUseFlags & useFlag))
                break;
            worldspace = world->mParent;
        }
        return worldspace;
    }

    const ESM4::World* getWorldspaceFeature(
        const Store<ESM4::World>& worlds, ESM::RefId worldspace, std::uint16_t useFlag)
    {
        return worlds.search(resolveWorldspaceInheritance(worlds, worldspace, useFlag));
    }
}
