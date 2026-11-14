#include "worldimp.hpp"
#include "class.hpp"
#include "inventorystore.hpp"
#include "oblivionscriptmanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwrender/renderingmanager.hpp"
#include "../mwrender/objects.hpp"
#include "../mwrender/vismask.hpp"
#include <components/esm4/loadingr.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/misc/convert.hpp>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace MWWorld
{
    std::optional<Ptr> World::dropOblivionInventoryIngredient(
        const Ptr& actor, const Ptr& item, size_t count, float cursorX, float cursorY)
    {
        // Bounded to the real scripted singleton course. Other item families
        // retain their existing admission paths until their own state is covered.
        const auto base = ESM::FormKey::content("oblivion.esm", 0x4e940);
        const ESM::FormKeyResolver resolver(mContentFiles);
        const auto id = resolver.toFormId(base);
        if (mGameProfile != ESM::GameProfile::Oblivion || !id || item.isEmpty()
            || item.getCellRef().getRefId() != ESM::RefId(*id))
            return std::nullopt;
        if (actor != getPlayerPtr() || !actor.isInCell() || count != 1
            || item.getCellRef().getCount() != 1 || !mOblivionScriptManager
            || item.getCellRef().getNativeScriptOwner().isNull()
            || item.getCellRef().getNativeBoundSource()
            || item.getCellRef().getNativeItemScale()
            || item.getCellRef().getNativeItemCondition()
            || item.getCellRef().getEnchantmentCharge() >= 0
            || item.getCellRef().getNativeOwnershipRank()
            || !item.getCellRef().getNativeOwnershipGlobal().empty()
            || item.getContainerStore() != &actor.getClass().getContainerStore(actor))
            throw std::invalid_argument("native Nirnroot drop requires its current scripted singleton");
        const auto scriptOwner = item.getCellRef().getNativeScriptOwner();
        const auto extraScale = item.getCellRef().getNativeItemScale();
        auto* cell = actor.getCell();
        const auto* definition = mStore.get<ESM4::Ingredient>().search(ESM::RefId(*id));
        const auto* cellDefinition = mStore.get<ESM4::Cell>().search(cell->getCell()->getId());
        const auto cellKey = mStore.get<ESM4::Cell>().findFormKey(cell->getCell()->getId());
        if (!definition || !cellDefinition || !cellKey || !mNextOblivionDynamicSerial
            || mNextOblivionDynamicSerial == std::numeric_limits<std::uint64_t>::max())
            throw std::invalid_argument("native Nirnroot drop has no winning content or identity");
        const auto serial = mNextOblivionDynamicSerial;
        const auto key = ESM::FormKey::dynamic("native-reference", serial);
        auto position = actor.getRefData().getPosition();
        const auto hit = mRendering->castCameraToViewportRay(cursorX, cursorY, 200.f, true, true, false);
        if (!hit.mHit)
            throw std::invalid_argument("native Nirnroot drop requires a visible placement surface");
        for (unsigned axis = 0; axis < 3; ++axis) position.pos[axis] = hit.mHitPointWorld[axis];
        position.rot[0] = position.rot[1] = 0;
        ESM4::Reference reference{};
        reference.mFormKey = key; reference.mBaseKey = base; reference.mBaseObj = definition->mId;
        reference.mParentKey = *cellKey; reference.mParent = cellDefinition->mId;
        reference.mPos = position; reference.mScale = item.getCellRef().getNativeItemScale().value_or(1.f);
        auto insertion = cell->prepareInsertion(LiveCellRef<ESM4::Ingredient>(reference, definition));
        const Ptr dropped(insertion->get(), cell);
        const std::array inserted{dropped};
        auto registration = mWorldModel.preparePtrReplacement({}, inserted);
        auto& inventory = actor.getClass().getInventoryStore(actor);
        auto removal = inventory.prepareItemRemoval(item, 1);
        auto transfer = mOblivionScriptManager->prepareItemScriptTransfer(
            base, scriptOwner, key);
        if (!mOblivionDynamicReferenceIdentity)
            mOblivionDynamicReferenceIdentity = std::make_shared<const char>();
        const std::weak_ptr<const char> identity = mOblivionDynamicReferenceIdentity;
        auto lua = MWBase::Environment::get().getLuaManager()->prepareSceneAdditions(inserted);
        const auto valid = [&] {
            return !identity.expired() && getPlayerPtr() == actor && actor.getCell() == cell
                && mNextOblivionDynamicSerial == serial && cell->validatePreparedInsertion(*insertion)
                // Inventory replacement can retire the borrowed item while the
                // World and Player remain current. Check its owner first.
                && removal->isValid()
                && item.getCellRef().getNativeScriptOwner() == scriptOwner
                && item.getCellRef().hasNativeItemScale(extraScale)
                && registration.isValid() && transfer->isValid() && lua->isValid();
        };
        auto& objects = mRendering->getObjects();
        const auto model = dropped.getClass().getCorrectedModel(dropped);
        const auto rotation = Misc::Convert::makeOsgQuat(position);
        auto view = objects.prepareModel(dropped, model, rotation, MWRender::Mask_Object, nullptr, valid);
        bool physics = false;
        try
        {
            if (!valid() || !objects.commitModel(*view))
                throw std::invalid_argument("native Nirnroot model admission became stale");
            physics = true;
            mPhysics->addObject(dropped, VFS::Path::Normalized(model), rotation);
            if (!valid() || !objects.validatePublishedModel(*view))
                throw std::invalid_argument("native Nirnroot drop became stale");
        }
        catch (...)
        {
            if (!identity.expired())
            {
                if (physics) mPhysics->remove(dropped);
                objects.rollbackModelAdmission(*view);
            }
            throw;
        }
        // All identities, nodes and script locals are allocated before debit.
        // No callbacks occur between the validated state publications.
        if (!transfer->commit() || !removal->commit())
            throw std::logic_error("validated native Nirnroot transfer rejected");
        if (!lua->commit())
            throw std::logic_error("validated native Nirnroot Lua admission rejected");
        cell->commitPreparedInsertion(*insertion);
        registration.commit();
        ++mNextOblivionDynamicSerial;
        removal->notify();
        if (identity.expired() || getPlayerPtr() != actor) return dropped;
        if (auto* windows = MWBase::Environment::get().getWindowManagerOrNull())
            windows->inventoryUpdated(actor);
        if (identity.expired() || getPlayerPtr() != actor) return dropped;
        if (mOblivionRuntimeState)
            mOblivionRuntimeState->mPlayer.mInventory = captureOblivionInventory(actor);
        MWBase::Environment::get().getLuaManager()->objectPlaced(dropped, actor, position.asVec3(), rotation);
        return dropped;
    }
}
