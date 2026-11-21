#include "obliviongate.hpp"
#include "esmstore.hpp"

#include <components/esm4/loadcell.hpp>
#include <components/esm4/loaddoor.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/loadwrld.hpp>

#include <map>
#include <set>
#include <vector>

namespace MWWorld
{
    namespace
    {
        bool inPlane(const ESMStore& store, const ESM4::Cell& cell)
        {
            const auto* world = store.search<ESM4::World>(cell.mParentKey);
            return cell.isExterior() && world && (world->mWorldFlags & ESM4::World::WLD_Oblivion);
        }

        ESM::RefId space(const ESM4::Cell& cell)
        {
            return cell.isExterior() ? cell.mParent : ESM::RefId(cell.mId);
        }
    }

    const ESM4::Reference* findOblivionGateExit(const ESMStore& store, const ESM4::Cell& origin)
    {
        if (origin.isExterior() && !inPlane(store, origin)) return nullptr;
        std::map<ESM::RefId, std::vector<ESM::RefId>> links;
        std::map<ESM::RefId, std::vector<const ESM4::Reference*>> gates;
        for (const auto& door : store.get<ESM4::Reference>())
        {
            if (door.mDoor.destDoor.isZeroOrUnset()) continue;
            const auto* other = store.get<ESM4::Reference>().search(door.mDoor.destDoor);
            const auto* from = store.search<ESM4::Cell>(door.mParentKey);
            const auto* to = other ? store.search<ESM4::Cell>(other->mParentKey) : nullptr;
            if (!from || !to || (from->isExterior() && !inPlane(store, *from))) continue;
            if (to->isExterior() && !inPlane(store, *to))
            {
                const auto* inside = store.search<ESM4::Door>(door.mBaseKey);
                const auto* outside = store.search<ESM4::Door>(other->mBaseKey);
                if (inPlane(store, *from) && other->mDoor.destDoor == door.mId && inside && outside
                    && (inside->mDoorFlags & ESM4::Door::Flag_OblivionGate)
                    && (outside->mDoorFlags & ESM4::Door::Flag_OblivionGate))
                    gates[space(*from)].push_back(&door);
            }
            else
                links[space(*from)].push_back(space(*to));
        }
        const ESM4::Reference* exit = nullptr;
        std::set<ESM::RefId> visited;
        std::vector<ESM::RefId> pending{space(origin)};
        while (!pending.empty())
        {
            const auto current = pending.back();
            pending.pop_back();
            if (!visited.insert(current).second) continue;
            if (const auto it = gates.find(current); it != gates.end())
                for (const auto* candidate : it->second)
                {
                    if (exit) return nullptr;
                    exit = candidate;
                }
            if (const auto it = links.find(current); it != links.end())
                pending.insert(pending.end(), it->second.begin(), it->second.end());
        }
        return exit;
    }
}
