#include "oblivionownership.hpp"
#include "worldimp.hpp"

#include <components/esm4/crimerules.hpp>
#include <components/esm4/loadfact.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <set>
#include <stdexcept>

namespace MWWorld
{
    namespace
    {
        const ESM4::Npc& nativePlayerBase(const ESMStore& store)
        {
            std::set<ESM::FormId> seen;
            for (const auto& entry : store.get<ESM4::Npc>())
            {
                if (!seen.insert(entry.mId).second) continue;
                const auto* winning = store.get<ESM4::Npc>().search(entry.mId);
                if (winning && Misc::StringUtils::ciEqual(winning->mEditorId, "Player"))
                    return *winning;
            }
            throw std::invalid_argument("native CELL claim cannot resolve the Player NPC base");
        }
    }

    ESM::FormId oblivionBipedReferenceOwner(World& world, const ConstPtr& reference)
    {
        if (world.getGameProfile() != ESM::GameProfile::Oblivion || reference.isEmpty()
            || (reference.getType() != ESM::REC_ARMO4 && reference.getType() != ESM::REC_CLOT4)
            || !reference.getCellRef().getNativeReference())
            throw std::invalid_argument("native reference owner requires a TES4 REFR");
        const auto& store = world.getStore();
        const auto ownerId = [&](const ESM::RefId& owner) -> ESM::FormId {
            if (owner.empty()) return {};
            const auto* id = owner.getIf<ESM::FormId>();
            if (!id || !store.hasEsm4ContentRecord(owner))
                throw std::invalid_argument("native reference owner is unresolved");
            return *id;
        };
        std::array<ESM::FormId,3> owners{};
        owners[0] = ownerId(reference.getCellRef().getOwner());
        const auto* placed = reference.getCellRef().getNativeReference();
        if (owners[0].isZeroOrUnset() && !placed->mDoor.destDoor.isZeroOrUnset())
        {
            const auto destination = world.getWorldModel().getPtr(placed->mDoor.destDoor);
            if (!destination.isEmpty())
                owners[1] = ownerId(destination.getCellRef().getOwner());
            else if (const auto* base = store.get<ESM4::Reference>().search(placed->mDoor.destDoor))
                owners[1] = ownerId(ESM::RefId(base->mOwner));
            else
                throw std::invalid_argument("native teleport ownership destination is unresolved");
        }
        if (owners[0].isZeroOrUnset() && owners[1].isZeroOrUnset() && reference.isInCell())
        {
            const auto* cell = store.get<ESM4::Cell>().search(reference.getCell()->getCell()->getId());
            if (!cell) throw std::invalid_argument("native reference ownership cell is unresolved");
            owners[2] = ownerId(ESM::RefId(cell->mOwner));
        }
        const auto layer = ESM4::ownershipOwnerLayer(ESM4::OwnershipReferenceKind::Other,
            {!owners[0].isZeroOrUnset(), !owners[1].isZeroOrUnset(), !owners[2].isZeroOrUnset()});
        return layer ? owners[*layer] : ESM::FormId{};
    }

    bool oblivionActorHasCellOwnershipClaim(World& world, const Ptr& actor, const CellStore& target)
    {
        if (world.getGameProfile() != ESM::GameProfile::Oblivion || actor.isEmpty()
            || !target.getCell()->isEsm4() || !world.getWorldModel().ownsCell(target))
            throw std::invalid_argument("native CELL claim requires its World, actor and managed TES4 cell");
        const auto& store = world.getStore();
        const auto* cell = store.get<ESM4::Cell>().search(target.getCell()->getId());
        const auto cellKey = store.get<ESM4::Cell>().findFormKey(target.getCell()->getId());
        if (!cell || !cellKey || !cellKey->isContent()
            || *cellKey != target.getCell()->getEsm4().mFormKey)
            throw std::invalid_argument("native CELL claim has an unresolved or stale CELL identity");

        const bool player = actor == world.getPlayerPtr();
        if (!player && (!actor.isInCell() || !world.getWorldModel().ownsCell(*actor.getCell())
            || world.getWorldModel().getPtr(actor.getCellRef().getRefNum()) != actor))
            throw std::invalid_argument("native CELL claim actor is not admitted to this World");
        const ESM4::Npc* base = nullptr;
        if (player) base = &nativePlayerBase(store);
        else if (actor.getType() == ESM::REC_NPC_4)
            base = actor.get<ESM4::Npc>()->mBase;
        else if (actor.getType() == ESM::REC_CREA4)
        {
            const auto* creature = actor.get<ESM4::Creature>()->mBase;
            if (!creature || store.get<ESM4::Creature>().search(creature->mId) != creature)
                throw std::invalid_argument("native CELL claim has an unresolved creature base");
            const auto key = store.get<ESM4::Creature>().findFormKey(creature->mId);
            if (!key || !key->isContent())
                throw std::invalid_argument("native CELL claim has no stable creature base identity");
            // Original TESNPC RTTI gate rejects a valid creature base.
            return false;
        }
        else
            throw std::invalid_argument("native CELL claim requires a native actor or actual Player");
        if (!base || !base->mIsTES4 || store.get<ESM4::Npc>().search(base->mId) != base)
            throw std::invalid_argument("native CELL claim has an unresolved or stale NPC base");
        const auto baseKey = store.get<ESM4::Npc>().findFormKey(base->mId);
        if (!baseKey || !baseKey->isContent())
            throw std::invalid_argument("native CELL claim has no stable NPC base identity");

        if (cell->mOwner.isZeroOrUnset()) return false;
        const ESM::RefId owner(cell->mOwner);
        if (!store.hasEsm4ContentRecord(owner))
            throw std::invalid_argument("native CELL claim has an unresolved owner");
        ESM4::OwnershipClaimInput input{};
        input.mRequiredFactionRank = cell->mOwnershipRank.value_or(-1);
        if (input.mRequiredFactionRank == -1) input.mRequiredFactionRank = 0;
        input.mUseFactionOwnership = true;
        input.mActorFactionRank = -1;
        if (const auto* npcOwner = store.get<ESM4::Npc>().search(owner))
        {
            input.mOwnerKind = ESM4::CrimeOwnerKind::Actor;
            input.mMatchesActorBase = npcOwner->mId == base->mId;
        }
        else if (const auto* faction = store.get<ESM4::Faction>().search(owner))
        {
            input.mOwnerKind = ESM4::CrimeOwnerKind::Faction;
            // Raw bit8 filters Player membership in this exact native query;
            // it is independent of the named Hidden/Evil/SpecialCombat flags.
            if (!player || !(faction->mFactionFlags.value_or(0) & 8))
                for (const auto& membership : base->mFactions)
                    if (ESM::FormId::fromUint32(membership.faction) == cell->mOwner)
                    {
                        input.mActorFactionRank = membership.rank;
                        break;
                    }
        }
        else
        {
            // A resolved non-NPC/non-FACT owner has no native claim. An
            // unresolved identity is unsupported data, not this valid branch.
            return false;
        }
        return ESM4::hasOwnershipClaim(input);
    }
}
