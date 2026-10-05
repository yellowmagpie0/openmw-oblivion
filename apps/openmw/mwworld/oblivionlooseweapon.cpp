#include "worldmodel.hpp"
#include "esmstore.hpp"

#include "../mwphysics/physicssystem.hpp"
#include "../mwrender/objects.hpp"
#include <components/esm4/loadweap.hpp>
#include <cmath>
#include <stdexcept>

namespace MWWorld
{
    struct WorldModel::PreparedLooseWeaponAdmission::Data
    {
        // Destroy the borrowed scene/physical preparations before their private
        // backing node. After publication the node instead belongs to CellStore.
        std::unique_ptr<CellStore::PreparedInsertion<ESM4::Weapon>> mCell;
        CellStore* mOwner = nullptr;
        Ptr mPtr;
        std::unique_ptr<PreparedPtrReplacement> mRegistry;
        MWRender::Objects* mObjects = nullptr;
        std::unique_ptr<MWRender::Objects::PreparedModel> mModel;
        MWPhysics::PhysicsSystem* mPhysics = nullptr;
        std::unique_ptr<MWPhysics::PreparedLooseObject> mBody;
        bool mConsumed = false;
    };

    WorldModel::PreparedLooseWeaponAdmission::PreparedLooseWeaponAdmission(std::unique_ptr<Data> data)
        : mData(std::move(data)) {}
    WorldModel::PreparedLooseWeaponAdmission::~PreparedLooseWeaponAdmission() = default;

    bool WorldModel::PreparedLooseWeaponAdmission::isValid() const
    {
        const auto* data = mData.get();
        // The registry lifetime guard runs before either the managed cell or
        // any borrowed owner/reference. CellStore is owned by that WorldModel.
        return data && !data->mConsumed && data->mRegistry->isValid()
            && data->mModel->hasLiveOwner() && data->mBody->hasLiveOwner()
            && data->mOwner->validatePreparedInsertion(*data->mCell)
            && data->mObjects->validatePreparedModel(*data->mModel)
            && data->mPhysics->validatePreparedLooseObject(*data->mBody);
    }

    Ptr WorldModel::PreparedLooseWeaponAdmission::commit()
    {
        return commit(LooseWeaponPublicationHooks{});
    }

    Ptr WorldModel::PreparedLooseWeaponAdmission::commit(const LooseWeaponPublicationHooks& hooks)
    {
        if (bool(hooks.mValidate) != bool(hooks.mPublish))
            throw std::invalid_argument("loose weapon compound publication requires both hooks");
        if (!isValid() || (hooks.mValidate && !hooks.mValidate(hooks.mContext)) || !isValid()) return {};
        auto& data = *mData;
        bool modelPublished = false, bodyPublished = false;
        try
        {
            if (!data.mObjects->commitModel(*data.mModel)) return {};
            modelPublished = true;
            // An external scene adapter can change the registry while adding
            // its child. Recheck logical ownership before physical admission.
            if (!data.mRegistry->isValid()
                || !data.mOwner->validatePreparedInsertion(*data.mCell)
                || !data.mBody->hasLiveOwner()
                || !data.mPhysics->commitLooseObject(*data.mBody))
                throw std::logic_error("loose weapon admission changed during scene publication");
            bodyPublished = true;
            // Source resource/key validation must follow all fallible scene and
            // physical admission. A stale source rolls both back, before source
            // inventory, serial, registry or cell publication.
            if ((hooks.mValidate && !hooks.mValidate(hooks.mContext))
                || !data.mRegistry->isValid() || !data.mModel->hasLiveOwner()
                || !data.mBody->hasLiveOwner()
                || !data.mOwner->validatePreparedInsertion(*data.mCell))
                throw std::logic_error("loose weapon compound inputs changed before publication");
            // After this final guard, the hook and existing cell/registry
            // commits allocate/dispatch nothing. Hook publication must leave
            // the registry and cell unchanged so their guards remain valid.
            if (hooks.mPublish) hooks.mPublish(hooks.mContext);
            // This guard can throw, but successful commit itself allocates and
            // dispatches nothing. Immediately splice the exact stable node.
            data.mRegistry->commit();
            data.mOwner->commitPreparedInsertion(*data.mCell);
            data.mConsumed = true;
            return data.mPtr;
        }
        catch (...)
        {
            if (bodyPublished && data.mBody->hasLiveOwner()) data.mPhysics->removeLooseObject(data.mPtr);
            if (modelPublished && data.mModel->hasLiveOwner()) data.mObjects->rollbackModelAdmission(*data.mModel);
            data.mConsumed = true;
            throw;
        }
    }

    std::unique_ptr<WorldModel::PreparedLooseWeaponAdmission> WorldModel::prepareLooseWeaponAdmission(
        CellStore& cell, const LiveCellRef<ESM4::Weapon>& reference,
        MWRender::Objects& objects, MWPhysics::PhysicsSystem& physics,
        const std::string& model, const osg::Quat& rotation, unsigned nodeMask,
        const NifBullet::ActorRagdollDefinition& definition, float lengthScale,
        std::span<const btTransform> poses, int collisionGroup, int collisionMask)
    {
        const auto found = mCells.find(cell.getCell()->getId());
        const auto* placed = reference.mRef.getNativeReference();
        const auto baseKey = reference.mBase
            ? mStore.get<ESM4::Weapon>().findFormKey(reference.mBase->mId) : std::nullopt;
        const auto cellKey = mStore.get<ESM4::Cell>().findFormKey(cell.getCell()->getId());
        if (found == mCells.end() || &found->second != &cell || !placed
            || !reference.mBase || mStore.get<ESM4::Weapon>().search(reference.mBase->mId) != reference.mBase
            || !baseKey || !baseKey->isContent() || !cellKey || !cellKey->isContent()
            || placed->mBaseObj != reference.mBase->mId || placed->mBaseKey != *baseKey
            || placed->mParent != cell.getCell()->getId() || placed->mParentKey != *cellKey
            || !placed->mId.isZeroOrUnset() || !placed->mFormKey.isDynamic()
            || placed->mFormKey.mValue == 0 || reference.mRef.getCount(false) != 1
            || reference.mRef.getScale() != 1.f || model.empty())
            throw std::invalid_argument("loose weapon admission requires an identified native instance and managed native cell");
        // A caller can construct a FormKey struct directly, bypassing the
        // validated factory. Do not publish an identity its save reader rejects.
        ESM::FormKey::dynamic(placed->mFormKey.mNamespace, placed->mFormKey.mValue);
        if (reference.isDeleted() || !reference.mData.isEnabled())
            throw std::invalid_argument("loose weapon admission requires a live enabled instance");
        if (const auto condition = reference.mRef.getNativeItemCondition();
            condition && (!std::isfinite(*condition) || *condition < 0
                || reference.mBase->mData.health == 0 || *condition > reference.mBase->mData.health))
            throw std::invalid_argument("loose weapon condition exceeds native base health");
        const auto charge = reference.mRef.getEnchantmentCharge();
        if (!std::isfinite(charge) || (charge != -1.f && (charge < 0
            || reference.mBase->mEnchantment.isZeroOrUnset()
            || charge > reference.mBase->mEnchantmentPoints)))
            throw std::invalid_argument("loose weapon charge exceeds native enchantment capacity");
        for (const auto& resident : getResidentPtrs())
            if (resident.getCellRef().getFormKey() == placed->mFormKey)
                throw std::invalid_argument("loose weapon stable identity already exists");
        auto data = std::make_unique<PreparedLooseWeaponAdmission::Data>();
        data->mCell = cell.prepareInsertion(reference);
        data->mOwner = &cell;
        data->mPtr = Ptr(data->mCell->get(), &cell);
        const std::array<Ptr, 1> inserted{data->mPtr};
        data->mRegistry = std::make_unique<PreparedPtrReplacement>(preparePtrReplacement({}, inserted));
        data->mObjects = &objects;
        data->mModel = objects.prepareModel(data->mPtr, model, rotation, nodeMask);
        data->mPhysics = &physics;
        data->mBody = physics.prepareLooseObject(data->mPtr, definition, lengthScale, poses,
            collisionGroup, collisionMask);
        return std::unique_ptr<PreparedLooseWeaponAdmission>(new PreparedLooseWeaponAdmission(std::move(data)));
    }
}
