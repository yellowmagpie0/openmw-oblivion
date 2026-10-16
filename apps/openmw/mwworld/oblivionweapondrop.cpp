#include "oblivionweapondrop.hpp"
#include "worldimp.hpp"
#include "inventorystore.hpp"
#include "oblivionownership.hpp"
#include "oblivionprofileservices.hpp"
#include "class.hpp"
#include "../mwrender/esm4npcanimation.hpp"
#include "../mwrender/vismask.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwphysics/collisiontype.hpp"
#include <components/esm4/dropplacement.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadfact.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/misc/convert.hpp>
#include <components/nifbullet/actorragdollphysics.hpp>
#include <bit>
#include <cmath>
#include <set>
#include <stdexcept>

namespace MWWorld
{
    namespace
    {
        bool sameFloat(std::optional<float> a, std::optional<float> b) noexcept
        {
            return a.has_value() == b.has_value()
                && (!a || std::bit_cast<std::uint32_t>(*a) == std::bit_cast<std::uint32_t>(*b));
        }
        ESM::FormId nativeOwner(const ESM::RefId& id)
        {
            if (id.empty()) return {};
            const auto* native = id.getIf<ESM::FormId>();
            if (!native) throw std::invalid_argument("weapon drop requires native ownership identities");
            return *native;
        }
    }

    struct PreparedEquippedWeaponDrops::Data
    {
        struct Entry
        {
            Ptr mActor, mItem;
            CellStore* mCell;
            MWRender::ESM4NpcAnimation* mAnimation;
            MWRender::ESM4NpcAnimation::CarriedWeaponDrop mCarried;
            CellRef mSource, mSourceAfter, mActorBefore, mActorAfter, mDestination;
            bool mHasWear;
            const ESM4::Npc* mNpc;
            const ESM4::Weapon* mWeapon;
            const ESM4::Cell* mCellBase;
            const ESM4::Npc* mCellNpcOwner;
            const ESM4::Faction* mCellFactionOwner;
            const ESM4::GlobalVariable* mGlobal;
            ESM::FormId mGlobalId;
            ESM::FormId mCellOwner;
            std::optional<std::int32_t> mCellRank;
            std::vector<std::pair<std::uint32_t, std::int8_t>> mFactions;
            std::string mWeaponModel;
            ESM::RefId mNpcId, mWeaponId, mCellId, mItemId;
            bool mActorEnabled;
            std::uint32_t mWeaponFlags, mWeaponType, mWeaponHealth;
            ESM::FormId mEnchantment;
            std::uint32_t mEnchantmentPoints;
            std::shared_ptr<InventoryStore::PreparedEquippedWeaponRemoval> mRemoval;
            std::unique_ptr<World::PreparedOblivionEquipmentNotification> mNotification;
        };
        World* mWorld;
        MWPhysics::PhysicsSystem* mPhysics;
        std::unique_ptr<World::PreparedOblivionDynamicReferenceKey> mKeys;
        std::vector<Entry> mEntries;
        std::unique_ptr<WorldModel::PreparedLooseWeaponAdmission> mAdmission;
        bool mConsumed = false;
        WorldModel::LooseWeaponPublicationHooks mHooks;

        bool current() const noexcept
        {
            if (mConsumed || !mKeys->isValid() || mWorld->getRayCasting() != mPhysics) return false;
            const auto& store = mWorld->getStore();
            if (mAdmission->references().size() != mEntries.size()) return false;
            for (std::size_t index = 0; index < mEntries.size(); ++index)
            {
                const auto& e = mEntries[index];
                // Tokens must be checked before any borrowed actor, item,
                // cell or animation can be dereferenced after external work.
                if (!e.mRemoval->isValid() || !e.mNotification->isValid()
                    || e.mCarried.mBody.mPose.mBindingIdentity.expired()) return false;
                const auto& source = e.mItem.getCellRef();
                const auto& destination = mAdmission->references()[index].getCellRef();
                const auto* placed = destination.getNativeReference();
                const auto* expected = e.mDestination.getNativeReference();
                if (mAdmission->references()[index].get<ESM4::Weapon>()->mBase != e.mWeapon
                    || !placed || placed->mId != expected->mId || placed->mFormKey != expected->mFormKey
                    || placed->mBaseObj != expected->mBaseObj || placed->mBaseKey != expected->mBaseKey
                    || placed->mParent != expected->mParent || placed->mParentKey != expected->mParentKey
                    || placed->mFlags != expected->mFlags || placed->mOwner != expected->mOwner
                    || placed->mGlobal != expected->mGlobal || placed->mFactionRank != expected->mFactionRank
                    || !sameFloat(placed->mScale, expected->mScale)
                    || !sameFloat(destination.getNativeItemCondition(), e.mDestination.getNativeItemCondition())
                    || !sameFloat(destination.getEnchantmentCharge(), e.mDestination.getEnchantmentCharge())
                    || destination.getNativeDropRelations() != e.mDestination.getNativeDropRelations()) return false;
                for (unsigned axis = 0; axis < 3; ++axis)
                    if (!sameFloat(placed->mPos.pos[axis], expected->mPos.pos[axis])
                        || !sameFloat(placed->mPos.rot[axis], expected->mPos.rot[axis])) return false;
                const auto* actor = e.mActor.getCellRef().getNativeActor();
                const auto* before = e.mActorBefore.getNativeActor();
                if (!actor || actor->mFormKey != before->mFormKey || actor->mBaseObj != before->mBaseObj
                    || actor->mFlags != before->mFlags || e.mActor.getCell() != e.mCell
                    || mWorld->getWorldModel().getPtr(e.mActor.getCellRef().getRefNum()) != e.mActor
                    || e.mActor.get<ESM4::Npc>()->mBase != e.mNpc
                    || store.get<ESM4::Npc>().search(e.mNpcId) != e.mNpc
                    || store.get<ESM4::Weapon>().search(e.mWeaponId) != e.mWeapon
                    || store.get<ESM4::Cell>().search(e.mCellId) != e.mCellBase
                    || e.mCellBase->mOwner != e.mCellOwner || e.mCellBase->mOwnershipRank != e.mCellRank
                    || store.get<ESM4::Npc>().search(ESM::RefId(e.mCellOwner)) != e.mCellNpcOwner
                    || store.get<ESM4::Faction>().search(ESM::RefId(e.mCellOwner)) != e.mCellFactionOwner
                    || store.get<ESM4::GlobalVariable>().search(ESM::RefId(e.mGlobalId)) != e.mGlobal
                    || e.mNpc->mFactions.size() != e.mFactions.size()
                    || source.getRefId() != e.mItemId
                    || e.mActor.getCellRef().getCount(false) != e.mActorBefore.getCount(false)
                    || e.mActor.getRefData().isEnabled() != e.mActorEnabled
                    || source.getOwner() != e.mSource.getOwner()
                    || source.getNativeOwnershipRank() != e.mSource.getNativeOwnershipRank()
                    || source.getNativeOwnershipGlobal() != e.mSource.getNativeOwnershipGlobal()
                    || !sameFloat(source.getNativeItemCondition(), e.mSource.getNativeItemCondition())
                    || source.getCharge() != e.mSource.getCharge()
                    || !sameFloat(source.getChargeIntRemainder(), e.mSource.getChargeIntRemainder())
                    || !sameFloat(source.getEnchantmentCharge(), e.mSource.getEnchantmentCharge())
                    || !source.hasNativeItemScale(e.mSource.getNativeItemScale())
                    || e.mActor.getCellRef().getNativeDropRelations() != e.mActorBefore.getNativeDropRelations()
                    || e.mWeapon->mModel.getOriginal() != e.mWeaponModel || e.mWeapon->mFlags != e.mWeaponFlags
                    || e.mWeapon->mData.type != e.mWeaponType || e.mWeapon->mData.health != e.mWeaponHealth
                    || e.mWeapon->mEnchantment != e.mEnchantment
                    || e.mWeapon->mEnchantmentPoints != e.mEnchantmentPoints
                    || !e.mAnimation->isCurrentCarriedWeaponDrop(e.mCarried, store)) return false;
                for (std::size_t i = 0; i < e.mFactions.size(); ++i)
                    if (e.mNpc->mFactions[i].faction != e.mFactions[i].first
                        || e.mNpc->mFactions[i].rank != e.mFactions[i].second) return false;
            }
            return true;
        }
        static bool validate(void* context) noexcept
        {
            auto& d = *static_cast<Data*>(context);
            return d.current() && (!d.mHooks.mValidate || d.mHooks.mValidate(d.mHooks.mContext)) && d.current();
        }
        static void publish(void* context) noexcept
        {
            auto& d = *static_cast<Data*>(context);
            if (d.mHooks.mPublish) d.mHooks.mPublish(d.mHooks.mContext);
            for (auto& e : d.mEntries)
            {
                if ((e.mHasWear && !e.mItem.getCellRef().swapNativeItemCondition(e.mSourceAfter))
                    || !e.mRemoval->commit() || !e.mActor.getCellRef().swapNativeDropRelations(e.mActorAfter))
                    std::terminate(); // Validated participants cannot fail in this callback-free interval.
            }
            if (!d.mKeys->commit()) std::terminate();
            for (auto& e : d.mEntries)
                if (!e.mNotification->commit()) std::terminate();
        }
    };

    PreparedEquippedWeaponDrops::PreparedEquippedWeaponDrops(std::unique_ptr<Data> data) : mData(std::move(data)) {}
    PreparedEquippedWeaponDrops::~PreparedEquippedWeaponDrops() = default;
    bool PreparedEquippedWeaponDrops::isValid() const
    { return mData && mData->current() && mData->mAdmission->isValid(); }
    std::span<const Ptr> PreparedEquippedWeaponDrops::references() const noexcept
    { return mData ? mData->mAdmission->references() : std::span<const Ptr>{}; }
    Ptr PreparedEquippedWeaponDrops::commit(const WorldModel::LooseWeaponPublicationHooks& hooks)
    {
        if (bool(hooks.mValidate) != bool(hooks.mPublish))
            throw std::invalid_argument("equipped weapon drop requires both compound hooks");
        if (!isValid()) return {};
        mData->mHooks = hooks;
        const Ptr result = mData->mAdmission->commit({mData.get(), Data::validate, Data::publish});
        mData->mConsumed = true;
        return result;
    }

    std::unique_ptr<PreparedEquippedWeaponDrops> prepareEquippedWeaponDrops(
        World& world, MWRender::Objects& objects, MWPhysics::PhysicsSystem& physics,
        std::span<const EquippedWeaponDrop> drops)
    {
        if (world.getGameProfile() != ESM::GameProfile::Oblivion || drops.empty()
            || world.getRayCasting() != &physics)
            throw std::invalid_argument("equipped weapon drop requires native World physics and nonempty sources");
        auto data = std::make_unique<PreparedEquippedWeaponDrops::Data>();
        data->mWorld = &world; data->mPhysics = &physics;
        data->mKeys = world.prepareOblivionDynamicReferenceKeys(drops.size());
        data->mEntries.reserve(drops.size());
        std::vector<LiveCellRef<ESM4::Weapon>> references;
        std::vector<std::vector<btTransform>> poses;
        std::vector<osg::Quat> rotations;
        references.reserve(drops.size()); poses.reserve(drops.size()); rotations.reserve(drops.size());
        std::set<Ptr> actors;
        auto& store = world.getStore();
        for (std::size_t i = 0; i < drops.size(); ++i)
        {
            const auto& request = drops[i];
            const auto& actor = request.mActor;
            const auto& item = request.mItem;
            if (actor.isEmpty() || actor == world.getPlayerPtr() || actor.getType() != ESM::REC_NPC_4
                || !actor.getCellRef().getNativeActor()
                || !actor.isInCell() || !world.getWorldModel().ownsCell(*actor.getCell())
                || world.getWorldModel().getPtr(actor.getCellRef().getRefNum()) != actor
                || !actors.insert(actor).second || request.mAnimation.getPtr() != actor)
                throw std::invalid_argument("weapon drop requires distinct admitted native NPCs and their animations");
            auto& inventory = actor.getClass().getInventoryStore(actor);
            const auto right = inventory.getSlot(InventoryStore::Slot_CarriedRight);
            if (item.isEmpty() || item.getType() != ESM::REC_WEAP || right == inventory.end() || *right != item)
                throw std::invalid_argument("weapon drop requires the exact carried-right weapon");
            const auto id = OblivionProfileServices::nativeItemId(store, item.getCellRef().getRefId());
            const auto* weapon = store.get<ESM4::Weapon>().search(id);
            const auto baseKey = store.get<ESM4::Weapon>().findFormKey(id);
            const auto* npc = actor.get<ESM4::Npc>()->mBase;
            const auto* cell = store.get<ESM4::Cell>().search(actor.getCell()->getCell()->getId());
            const auto cellKey = store.get<ESM4::Cell>().findFormKey(actor.getCell()->getCell()->getId());
            auto carried = request.mAnimation.captureCarriedWeaponDrop(item, store);
            if (!weapon || !baseKey || !baseKey->isContent() || !npc || !cell || !cellKey
                || !cellKey->isContent() || !carried || !carried->mBody.mDefinition)
                throw std::invalid_argument("weapon drop cannot resolve its winning records or bound carried NIF");
            auto removal = std::shared_ptr<InventoryStore::PreparedEquippedWeaponRemoval>(inventory.prepareEquippedWeaponRemoval());
            if (!removal || !removal->isValid())
                throw std::invalid_argument("weapon drop cannot prepare its exact source debit");
            auto notification = world.prepareOblivionEquipmentNotification([removal] { removal->notify(); });
            const CellRef& source = item.getCellRef();
            ESM4::Reference placed{};
            placed.mFormKey = data->mKeys->keys()[i]; placed.mBaseObj = weapon->mId; placed.mBaseKey = *baseKey;
            placed.mParent = cell->mId; placed.mParentKey = *cellKey;
            placed.mPos = carried->mAnchor.getDropPlacement().mPosition;
            placed.mScale = ESM4::droppedReferenceScale(source.getNativeItemScale());
            placed.mOwner = nativeOwner(source.getOwner());
            placed.mGlobal = nativeOwner(source.getNativeOwnershipGlobal());
            const auto* global = store.get<ESM4::GlobalVariable>().search(ESM::RefId(placed.mGlobal));
            const auto globalKey = store.get<ESM4::GlobalVariable>().findFormKey(ESM::RefId(placed.mGlobal));
            if (!placed.mGlobal.isZeroOrUnset() && (!global || !globalKey || !globalKey->isContent()))
                throw std::invalid_argument("weapon drop has an unresolved native ownership global");
            placed.mFactionRank = source.getNativeOwnershipRank();
            if (placed.mFactionRank == -1) placed.mFactionRank.reset();
            if (placed.mOwner.isZeroOrUnset() && !cell->mOwner.isZeroOrUnset()
                && !oblivionActorHasCellOwnershipClaim(world, actor, *actor.getCell())) placed.mOwner = npc->mId;
            references.emplace_back(placed, weapon);
            auto& reference = references.back();
            auto condition = request.mConditionAfterWear ? request.mConditionAfterWear : source.getNativeItemCondition();
            if (!condition && source.getCharge() >= 0)
                condition = static_cast<float>(double(source.getCharge()) + source.getChargeIntRemainder());
            if (condition) reference.mRef.setNativeItemCondition(*condition);
            reference.mRef.setEnchantmentCharge(source.getEnchantmentCharge());
            CellRef sourceAfter = source;
            if (request.mConditionAfterWear) sourceAfter.setNativeItemCondition(*request.mConditionAfterWear);
            CellRef actorAfter = actor.getCellRef();
            if (!(weapon->mFlags & 0x400))
            {
                auto relations = actorAfter.getNativeDropRelations();
                if (!relations.mItems) relations.mItems.emplace();
                relations.mItems->insert(relations.mItems->begin(), placed.mFormKey);
                actorAfter.setNativeDropRelations(std::move(relations));
                ESM4::DropRelations dropped; dropped.mDropper = actor.getCellRef().getFormKey();
                reference.mRef.setNativeDropRelations(std::move(dropped));
            }
            rotations.push_back(Misc::Convert::makeOsgQuat(placed.mPos));
            const osg::Matrixf root = osg::Matrixf::rotate(rotations.back()) * osg::Matrixf::translate(placed.mPos.asVec3());
            const auto scaled = NifBullet::ragdollDefinitionWithNativeScaledProperties(*carried->mBody.mDefinition, placed.mScale);
            std::vector<NifBullet::RagdollBoneWorldPose> bones;
            for (const auto& body : scaled.mBodies)
            {
                auto bind = body.mBoneBind;
                bind.setTrans(bind.getTrans() * placed.mScale);
                bones.push_back({body.mNodeRecord, bind * root});
            }
            poses.push_back(NifBullet::ragdollBodyWorldPoses(scaled, bones));
            PreparedEquippedWeaponDrops::Data::Entry entry{
                actor, item, actor.getCell(), &request.mAnimation, std::move(*carried), source, std::move(sourceAfter),
                actor.getCellRef(), std::move(actorAfter), reference.mRef, request.mConditionAfterWear.has_value(), npc, weapon, cell,
                store.get<ESM4::Npc>().search(ESM::RefId(cell->mOwner)),
                store.get<ESM4::Faction>().search(ESM::RefId(cell->mOwner)), global, placed.mGlobal, cell->mOwner, cell->mOwnershipRank,
                {}, weapon->mModel.getOriginal(), ESM::RefId(npc->mId), ESM::RefId(weapon->mId), cell->mId,
                source.getRefId(), actor.getRefData().isEnabled(), weapon->mFlags, weapon->mData.type, weapon->mData.health,
                weapon->mEnchantment, weapon->mEnchantmentPoints, std::move(removal), std::move(notification)};
            for (const auto& faction : npc->mFactions) entry.mFactions.emplace_back(faction.faction, faction.rank);
            data->mEntries.push_back(std::move(entry));
        }
        std::vector<WorldModel::LooseWeaponAdmission> admissions;
        for (std::size_t i = 0; i < drops.size(); ++i)
        {
            const auto& e = data->mEntries[i];
            admissions.push_back({*e.mCell, references[i], objects, physics, std::string(e.mCarried.mBody.mPose.mModel.value()),
                rotations[i], MWRender::Mask_Object, *e.mCarried.mBody.mDefinition,
                NifBullet::RagdollNativeLengthScale, poses[i], MWPhysics::CollisionType_World,
                MWPhysics::CollisionType_AnyPhysical});
        }
        data->mAdmission = world.getWorldModel().prepareLooseWeaponAdmissions(admissions);
        // Native REFR.mId is also the engine-local RefNum. The registry
        // reservation assigns it on the private nodes, independently of the
        // stable dynamic key; guard that assigned identity from this point.
        for (std::size_t i = 0; i < data->mEntries.size(); ++i)
            data->mEntries[i].mDestination.setRefNum(data->mAdmission->references()[i].getCellRef().getRefNum());
        return std::unique_ptr<PreparedEquippedWeaponDrops>(new PreparedEquippedWeaponDrops(std::move(data)));
    }
}
