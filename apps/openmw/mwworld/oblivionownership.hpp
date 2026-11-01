#ifndef OPENMW_MWWORLD_OBLIVIONOWNERSHIP_HPP
#define OPENMW_MWWORLD_OBLIVIONOWNERSHIP_HPP

#include <components/esm/formid.hpp>

namespace ESM4 { struct Npc; }

namespace MWWorld
{
    class ESMStore;
    // Winning native Player metadata; actor-value overrides keep their independent dynamic base.
    const ESM4::Npc& oblivionPlayerBase(const ESMStore& store);
    class World;
    class Ptr;
    class ConstPtr;
    // Read-only native ARMO/CLOT REFR owner resolution, including direct teleport owner
    // and current-cell inheritance. Does not create actor inventory/custom data.
    ESM::FormId oblivionBipedReferenceOwner(World& world, const ConstPtr& reference);
    class CellStore;
    // Positive native CELL claim, not legality/access or permission-global
    // policy. Reads winning CELL/NPC_/FACT records and static base membership.
    // Native faction mutation/persistence is a separate pending authority;
    // do not route mutable faction commands through a parallel TES3 map.
    // Unknown/stale bindings reject rather than becoming unowned/no-claim.
    bool oblivionActorHasCellOwnershipClaim(World& world, const Ptr& actor, const CellStore& cell);
}

#endif
