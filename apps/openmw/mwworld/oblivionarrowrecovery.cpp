#include "oblivionarrowrecovery.hpp"

#include "worldimp.hpp"
#include "esmstore.hpp"
#include "class.hpp"
#include "savedreference.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwrender/objects.hpp"
#include "../mwrender/vismask.hpp"
#include "../mwrender/renderingmanager.hpp"

#include <components/esm4/loadammo.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/loadstat.hpp>
#include <components/esm4/runtimestate.hpp>
#include <components/esm4/dropplacement.hpp>
#include <components/misc/convert.hpp>
#include <components/misc/resourcehelpers.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace MWWorld
{
    namespace
    {
        constexpr std::array<std::string_view, 7> fields{
            "native.arrow.bow", "native.arrow.caster", "native.arrow.surface", "native.arrow.projectile",
            "native.arrow.age", "native.arrow.opacity", "native.arrow.fading"};
        void validate(const RecoveredArrowState& state)
        {
            if (!state.mBow.isContent() || !state.mSurface.isContent()
                || state.mCaster != ESM::FormKey::dynamic("player", 1) || !state.mProjectile)
                throw std::invalid_argument("invalid recovered arrow identity");
            for (const auto& key : {state.mBow, state.mSurface})
                if (ESM::FormKey::deserialize(key.serialize()) != key)
                    throw std::invalid_argument("noncanonical recovered arrow identity");
            (void)ESM4::advanceArrowLifetime(state.mLifetime, 0, {90});
        }
    }

    std::optional<RecoveredArrowState> readRecoveredArrow(const ESM4::RuntimeReferenceState& reference)
    {
        const auto& values = reference.mCustomState;
        const auto count = std::count_if(values.begin(), values.end(),
            [](const auto& entry) { return entry.first.starts_with("native.arrow."); });
        if (!count) return {};
        if (static_cast<std::size_t>(count) != fields.size()
            || std::any_of(fields.begin(), fields.end(), [&](auto key) { return !values.contains(key); }))
            throw std::invalid_argument("incomplete recovered arrow ownership");
        const auto text = [&](std::size_t index) -> const std::string& {
            const auto* value = std::get_if<std::string>(&values.find(fields[index])->second);
            if (!value) throw std::invalid_argument("invalid recovered arrow text");
            return *value;
        };
        const auto key = [&](std::size_t index) {
            const auto& value = text(index);
            auto result = ESM::FormKey::deserialize(value);
            if (result.serialize() != value) throw std::invalid_argument("noncanonical recovered arrow key");
            return result;
        };
        const auto number = [&](std::size_t index) {
            const auto& value = values.find(fields[index])->second;
            const auto* real = std::get_if<double>(&value);
            const auto* integer = std::get_if<std::int64_t>(&value);
            if (!real && !integer) throw std::invalid_argument("invalid recovered arrow number");
            const double result = real ? *real : double(*integer);
            if (!std::isfinite(result) || std::abs(result) > std::numeric_limits<float>::max())
                throw std::invalid_argument("nonfinite recovered arrow number");
            return static_cast<float>(result);
        };
        const auto* fading = std::get_if<bool>(&values.find(fields[6])->second);
        if (!fading) throw std::invalid_argument("invalid recovered arrow fade flag");
        const auto& serial = text(3);
        std::uint64_t id = 0;
        const auto result = std::from_chars(serial.data(), serial.data() + serial.size(), id);
        if (result.ec != std::errc{} || result.ptr != serial.data() + serial.size() || std::to_string(id) != serial)
            throw std::invalid_argument("noncanonical recovered arrow serial");
        RecoveredArrowState state{key(0), key(1), key(2), id, {number(4), number(5), *fading}};
        validate(state);
        return state;
    }

    void writeRecoveredArrow(ESM4::RuntimeReferenceState& reference, const RecoveredArrowState& state)
    {
        validate(state);
        auto values = reference.mCustomState;
        std::erase_if(values, [](const auto& entry) { return entry.first.starts_with("native.arrow."); });
        values.emplace(fields[0], state.mBow.serialize());
        values.emplace(fields[1], state.mCaster.serialize());
        values.emplace(fields[2], state.mSurface.serialize());
        values.emplace(fields[3], std::to_string(state.mProjectile));
        values.emplace(fields[4], double(state.mLifetime.mAge));
        values.emplace(fields[5], double(state.mLifetime.mOpacity));
        values.emplace(fields[6], state.mLifetime.mFading);
        reference.mCustomState.swap(values);
    }

    void validateRecoveredArrowSource(const ESMStore& store, const ESM4::RuntimeReferenceState& reference,
        const RecoveredArrowState& state)
    {
        validate(state);
        const auto* ammo = store.get<ESM4::Ammunition>().search(reference.mBase);
        const auto* bow = store.get<ESM4::Weapon>().search(state.mBow);
        const auto* surface = store.get<ESM4::Reference>().search(state.mSurface);
        const auto* stone = surface ? store.get<ESM4::Static>().search(ESM::RefId(surface->mBaseObj)) : nullptr;
        if (!reference.mKey.isDynamic() || reference.mKey.mNamespace != "native-reference"
            || !ammo || (ammo->mFlags & ESM4::Rec_Deleted) || ammo->mData.mFlags != 0
            || !ammo->mEnchantment.isZeroOrUnset() || !ammo->mAmmoEffects.empty() || ammo->mModel.empty()
            || !bow || (bow->mFlags & ESM4::Rec_Deleted) || bow->mData.type != 5
            || !bow->mEnchantment.isZeroOrUnset() || !surface || (surface->mFlags & ESM4::Rec_Deleted)
            || !stone || (stone->mFlags & ESM4::Rec_Deleted)
            || (!surface->mParentKey.isNull() && surface->mParentKey != reference.mCell))
            throw std::invalid_argument("recovered arrow source no longer resolves");
    }

    struct PreparedArrowRecovery::Data
    {
        World& mWorld;
        MWRender::Objects& mObjects;
        MWPhysics::PhysicsSystem& mPhysics;
        CellStore& mCell;
        const ESM4::Ammunition* mAmmo;
        std::unique_ptr<World::PreparedOblivionDynamicReferenceKey> mKey;
        std::unique_ptr<PreparedSavedNativeReference> mInsertion;
        std::unique_ptr<WorldModel::PreparedPtrReplacement> mRegistry;
        std::unique_ptr<MWBase::LuaManager::PreparedSceneAdditions> mLua;
        std::unique_ptr<MWRender::Objects::PreparedModel> mModel;
        Ptr mPtr;
        ESM4::RuntimeReferenceState mReference;
        osg::Quat mRotation;
        std::string mMesh;
        bool mModelPublished = false, mBodyPublished = false, mCommitted = false;

        Data(World& world, MWRender::Objects& objects, MWPhysics::PhysicsSystem& physics,
            CellStore& cell, const ESM4::Ammunition* ammo)
            : mWorld(world), mObjects(objects), mPhysics(physics), mCell(cell), mAmmo(ammo) {}
        bool current() const noexcept
        {
            return !mCommitted && mKey && mKey->isValid() && mInsertion && mInsertion->isValid()
                && mRegistry && mRegistry->isValid() && mLua && mLua->isValid()
                && mWorld.getRayCasting() == &mPhysics;
        }
        ~Data()
        {
            if (mCommitted) return;
            if (mBodyPublished) mPhysics.remove(mPtr);
            if (mModelPublished) mObjects.rollbackModelAdmission(*mModel);
        }
    };

    PreparedArrowRecovery::PreparedArrowRecovery(std::unique_ptr<Data> data) : mData(std::move(data)) {}
    PreparedArrowRecovery::~PreparedArrowRecovery() = default;
    const ESM4::RuntimeReferenceState& PreparedArrowRecovery::reference() const { return mData->mReference; }

    Ptr PreparedArrowRecovery::commit(void* context, bool (*sourceValid)(void*) noexcept,
        void (*publish)(void*) noexcept)
    {
        auto& d = *mData;
        if (!sourceValid || !publish || !d.current() || !sourceValid(context)) return {};
        if (!d.mObjects.validatePreparedModel(*d.mModel) || !d.mObjects.commitModel(*d.mModel)) return {};
        d.mModelPublished = true;
        d.mBodyPublished = true; // remove() is also safe when admission throws before insertion.
        d.mPhysics.addObject(d.mPtr, VFS::Path::Normalized(d.mMesh), d.mRotation);
        if (!d.current() || !sourceValid(context) || !d.mObjects.validatePublishedModel(*d.mModel)) return {};
        // Every remaining write was prepared. No observer can see both a
        // surviving flight and a newly recoverable item, or a reused identity.
        if (!d.mKey->commit() || !d.mLua->commit()) std::terminate();
        publish(context);
        d.mInsertion->commit();
        d.mRegistry->commit();
        d.mCommitted = true;
        return d.mPtr;
    }

    std::unique_ptr<PreparedArrowRecovery> prepareArrowRecovery(World& world, MWRender::Objects& objects,
        MWPhysics::PhysicsSystem& physics, const Ptr& surface, const ESM4::ArrowProjectileState& arrow,
        std::uint64_t serial, const osg::Vec3f& hit)
    {
        if (surface.isEmpty() || surface.getType() != ESM::REC_STAT4 || !surface.isInCell()
            || !surface.getCellRef().getFormKey().isContent()) return {};
        const auto* ammo = world.getStore().get<ESM4::Ammunition>().search(arrow.mAmmunition);
        if (!ammo) throw std::invalid_argument("recovered arrow ammunition vanished");
        auto data = std::make_unique<PreparedArrowRecovery::Data>(world, objects, physics, *surface.getCell(), ammo);
        auto& d = *data;
        d.mKey = world.prepareOblivionDynamicReferenceKey();
        osg::Vec3f direction(arrow.mVelocity[0], arrow.mVelocity[1], arrow.mVelocity[2]);
        if (direction.normalize() == 0) throw std::invalid_argument("recovered arrow has no direction");
        osg::Quat rotation; rotation.makeRotate(osg::Vec3f(0, 1, 0), direction);
        const auto matrix = osg::Matrixf::rotate(rotation);
        std::array<float, 9> basis;
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned column = 0; column < 3; ++column) basis[row * 3 + column] = matrix(column, row);
        // Small backoff prevents the ray-contact point from burying the whole
        // pickable mesh. Full Havok penetration/ricochet is a separate course.
        const osg::Vec3f position = hit - direction * 2.f;
        const auto placement = ESM4::physicalDroppedItemPlacement(basis, {position.x(), position.y(), position.z()});
        ESM4::Reference placed{};
        placed.mFormKey = d.mKey->key(); placed.mBaseObj = ammo->mId; placed.mBaseKey = arrow.mAmmunition;
        placed.mParent = d.mCell.getCell()->getId();
        placed.mParentKey = d.mCell.getCell()->getEsm4().mFormKey;
        placed.mPos = placement.mPosition; placed.mScale = 1;
        d.mInsertion = prepareSavedNativeReference(world.getStore(), d.mCell, placed);
        d.mPtr = d.mInsertion->get();
        d.mPtr.getCellRef().setCount(1);
        const std::array<Ptr, 1> inserted{d.mPtr};
        d.mRegistry = std::make_unique<WorldModel::PreparedPtrReplacement>(
            world.getWorldModel().preparePtrReplacement({}, inserted));
        d.mLua = MWBase::Environment::get().getLuaManager()->prepareSceneAdditions(inserted);
        d.mReference.mKey = placed.mFormKey; d.mReference.mBase = placed.mBaseKey;
        d.mReference.mCell = placed.mParentKey; d.mReference.mPosition = placed.mPos;
        d.mReference.mCustomState["count"] = std::int64_t(1);
        d.mReference.mCustomState["scale"] = 1.;
        d.mReference.mCustomState["record_type"] = std::int64_t(ESM::REC_AMMO4);
        writeRecoveredArrow(d.mReference, {arrow.mBow, arrow.mCaster, surface.getCellRef().getFormKey(), serial, {}});
        validateRecoveredArrowSource(world.getStore(), d.mReference, *readRecoveredArrow(d.mReference));
        d.mMesh = Misc::ResourceHelpers::correctMeshPath(ammo->mModel.getNormalized()).value();
        d.mRotation = Misc::Convert::makeOsgQuat(placed.mPos);
        d.mModel = objects.prepareModel(d.mPtr, d.mMesh, d.mRotation, MWRender::Mask_Object, nullptr,
            [ptr = data.get()] { return ptr->current(); });
        return std::unique_ptr<PreparedArrowRecovery>(new PreparedArrowRecovery(std::move(data)));
    }
}
