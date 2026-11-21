#ifndef OPENMW_MWWORLD_OBLIVIONGATE_HPP
#define OPENMW_MWWORLD_OBLIVIONGATE_HPP

namespace ESM4 { struct Cell; struct Reference; struct RuntimeReferenceState; }

namespace MWWorld
{
    class ESMStore;

    bool isDestroyedOblivionGate(const ESM4::RuntimeReferenceState& gate);
    bool needsOblivionGateCollapse(const ESM4::RuntimeReferenceState& gate);

    // Inside door of a unique authored pair reachable from this plane.
    const ESM4::Reference* findOblivionGateExit(const ESMStore& store, const ESM4::Cell& origin);
}

#endif
