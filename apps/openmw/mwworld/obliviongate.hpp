#ifndef OPENMW_MWWORLD_OBLIVIONGATE_HPP
#define OPENMW_MWWORLD_OBLIVIONGATE_HPP

#include <components/esm/formkey.hpp>

namespace ESM4 { struct Cell; struct Reference; }

namespace MWWorld
{
    class ESMStore;

    // Inside door of a unique authored pair reachable from this plane.
    const ESM4::Reference* findOblivionGateExit(const ESMStore& store, const ESM4::Cell& origin);
}

#endif
