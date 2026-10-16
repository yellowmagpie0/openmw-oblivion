#include "worldmodel.hpp"
#include "esmstore.hpp"

#include "../mwphysics/physicssystem.hpp"
#include "../mwrender/objects.hpp"
#include <components/esm4/loadweap.hpp>
#include <components/esm4/dropplacement.hpp>
#include <components/nifbullet/actorragdoll.hpp>
#include <cmath>
#include <set>
#include <stdexcept>

namespace MWWorld
{
    struct WorldModel::PreparedLooseWeaponAdmission::Data
    {
        struct Entry
        {
            // Destroy borrowed resource preparations before their private node.
            std::unique_ptr<CellStore::PreparedInsertion<ESM4::Weapon>> mCell;
            CellStore* mOwner = nullptr;
            MWRender::Objects* mObjects = nullptr;
            std::unique_ptr<MWRender::Objects::PreparedModel> mModel;
            MWPhysics::PhysicsSystem* mPhysics = nullptr;
            std::unique_ptr<MWPhysics::PreparedLooseObject> mBody;
            bool mModelPublished = false, mBodyPublished = false;
        };
        std::vector<Entry> mEntries;
        std::vector<Ptr> mReferences;
        std::unique_ptr<PreparedPtrReplacement> mRegistry;
        bool mConsumed = false;

        bool current() const
        {
            // Retire borrowed cells/references before any dereference after
            // World destruction, clear or a scene callback changing registry.
            if (mConsumed || !mRegistry->isValid()) return false;
            for (std::size_t i = 0; i < mEntries.size(); ++i)
            {
                const auto& entry = mEntries[i];
                if (!entry.mModel->hasLiveOwner() || !entry.mBody->hasLiveOwner()
                    || !entry.mOwner->validatePreparedInsertion(*entry.mCell)
                    || (entry.mModelPublished
                        ? !entry.mObjects->validatePublishedModel(*entry.mModel)
                        : !entry.mObjects->validatePreparedModel(*entry.mModel))
                    || (entry.mBodyPublished
                        ? !entry.mPhysics->hasLooseObject(mReferences[i])
                        : !entry.mPhysics->validatePreparedLooseObject(*entry.mBody)))
                    return false;
            }
            return true;
        }

        void rollback()
        {
            // Reverse order also removes shared-cell children before its root.
            for (std::size_t i = mEntries.size(); i-- > 0;)
            {
                auto& entry = mEntries[i];
                if (entry.mBodyPublished && entry.mBody->hasLiveOwner())
                    entry.mPhysics->removeLooseObject(mReferences[i]);
                if (entry.mModelPublished && entry.mModel->hasLiveOwner())
                    entry.mObjects->rollbackModelAdmission(*entry.mModel);
            }
            mConsumed = true;
        }
    };

    WorldModel::PreparedLooseWeaponAdmission::PreparedLooseWeaponAdmission(std::unique_ptr<Data> data)
        : mData(std::move(data)) {}
    WorldModel::PreparedLooseWeaponAdmission::~PreparedLooseWeaponAdmission() = default;

    bool WorldModel::PreparedLooseWeaponAdmission::isValid() const
    {
        return mData && mData->current();
    }

    std::span<const Ptr> WorldModel::PreparedLooseWeaponAdmission::references() const noexcept
    {
        return mData ? std::span<const Ptr>(mData->mReferences) : std::span<const Ptr>{};
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
        try
        {
            // All external scene admissions precede every physical admission.
            // A later scene callback can invalidate an earlier model or any
            // source without encountering a partly published physical group.
            for (std::size_t i = 0; i < data.mEntries.size(); ++i)
            {
                auto& entry = data.mEntries[i];
                const MWRender::Objects::PreparedModel* sibling = nullptr;
                for (std::size_t j = 0; j < i; ++j)
                    if (data.mEntries[j].mOwner == entry.mOwner && data.mEntries[j].mObjects == entry.mObjects)
                    {
                        sibling = data.mEntries[j].mModel.get();
                        break;
                    }
                // Pending roots in the same cell become valid after rebasing
                // onto the transaction's admitted sibling, inside commitModel.
                if (!data.mRegistry->isValid() || !entry.mModel->hasLiveOwner()
                    || !entry.mBody->hasLiveOwner()
                    || !entry.mOwner->validatePreparedInsertion(*entry.mCell)
                    || !entry.mObjects->commitModel(*entry.mModel, sibling))
                    throw std::logic_error("loose weapon group changed during scene publication");
                entry.mModelPublished = true;
            }
            if (!data.current())
                throw std::logic_error("loose weapon group changed after scene publication");
            for (auto& entry : data.mEntries)
            {
                if (!entry.mPhysics->commitLooseObject(*entry.mBody))
                    throw std::logic_error("loose weapon group physical admission rejected");
                entry.mBodyPublished = true;
            }
            // Final callback-free validation precedes all source/contact/serial
            // writes. One hook and registry swap publish the entire group.
            if (!data.current() || (hooks.mValidate && !hooks.mValidate(hooks.mContext)) || !data.current())
                throw std::logic_error("loose weapon compound inputs changed before publication");
            if (hooks.mPublish) hooks.mPublish(hooks.mContext);
            data.mRegistry->commit();
            for (auto& entry : data.mEntries) entry.mOwner->commitPreparedInsertion(*entry.mCell);
            data.mConsumed = true;
            return data.mReferences.front();
        }
        catch (...)
        {
            data.rollback();
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
        const std::array<LooseWeaponAdmission, 1> admissions{{
            {cell, reference, objects, physics, model, rotation, nodeMask, definition, lengthScale,
                poses, collisionGroup, collisionMask}}};
        return prepareLooseWeaponAdmissions(admissions);
    }

    std::unique_ptr<WorldModel::PreparedLooseWeaponAdmission> WorldModel::prepareLooseWeaponAdmissions(
        std::span<const LooseWeaponAdmission> admissions)
    {
        if (admissions.empty()) throw std::invalid_argument("loose weapon group must not be empty");
        auto data = std::make_unique<PreparedLooseWeaponAdmission::Data>();
        data->mEntries.reserve(admissions.size());
        data->mReferences.reserve(admissions.size());
        const auto residents = getResidentPtrs();
        std::set<ESM::FormKey> keys;
        for (const auto& admission : admissions)
        {
            auto& cell = admission.mCell;
            const auto& reference = admission.mReference;
            const auto& model = admission.mModel;
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
                || model.empty())
                throw std::invalid_argument("loose weapon admission requires an identified native instance and managed native cell");
            const float scale = reference.mRef.getScale();
            if (ESM4::droppedReferenceScale(scale) != scale)
                throw std::invalid_argument("loose weapon admission requires a resolved native reference scale");
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
            for (const auto& resident : residents)
                if (resident.getCellRef().getFormKey() == placed->mFormKey)
                    throw std::invalid_argument("loose weapon stable identity already exists");
            if (!keys.insert(placed->mFormKey).second)
                throw std::invalid_argument("loose weapon group has duplicate stable identities");
            auto& entry = data->mEntries.emplace_back();
            entry.mCell = cell.prepareInsertion(reference);
            entry.mOwner = &cell;
            entry.mObjects = &admission.mObjects;
            entry.mPhysics = &admission.mPhysics;
            data->mReferences.emplace_back(entry.mCell->get(), &cell);
        }
        // Assign all local RefNums from one private registry copy, preserving
        // public counters and all original caller references on cancellation.
        data->mRegistry = std::make_unique<PreparedPtrReplacement>(preparePtrReplacement({}, data->mReferences));
        for (std::size_t i = 0; i < admissions.size(); ++i)
        {
            const auto& admission = admissions[i];
            auto& entry = data->mEntries[i];
            const auto& ptr = data->mReferences[i];
            entry.mModel = entry.mObjects->prepareModel(ptr, admission.mModel, admission.mRotation, admission.mNodeMask,
                admission.mDefinition.mSourceHash.empty() ? nullptr : &admission.mDefinition);
            const float scale = ptr.getCellRef().getScale();
            std::optional<NifBullet::ActorRagdollDefinition> scaled;
            if (scale != 1.f)
                scaled = NifBullet::ragdollDefinitionWithNativeScaledProperties(admission.mDefinition, scale);
            entry.mBody = entry.mPhysics->prepareLooseObject(ptr, scaled ? *scaled : admission.mDefinition,
                admission.mLengthScale, admission.mPoses, admission.mCollisionGroup, admission.mCollisionMask,
                entry.mModel->looseSceneBinding());
        }
        return std::unique_ptr<PreparedLooseWeaponAdmission>(new PreparedLooseWeaponAdmission(std::move(data)));
    }
}
