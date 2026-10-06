#ifndef OPENMW_MWWORLD_SAVEDREFERENCE_H
#define OPENMW_MWWORLD_SAVEDREFERENCE_H

#include <memory>
#include <cstdint>

namespace ESM
{
    class ESMReader;
    struct CellRef;
    struct ObjectState;
}

namespace MWWorld
{
    // Decode with CellStore's record/state mapping into detached data. Does
    // not load cells, construct live classes, register references or publish.
    std::unique_ptr<ESM::ObjectState> readSavedReferenceState(
        ESM::ESMReader& reader, const ESM::CellRef& reference, std::uint32_t type);
}

#endif
