#include "oblivionweapondrop.hpp"
#include "worldimp.hpp"
#include "inventorystore.hpp"
#include "oblivionownership.hpp"
#include "oblivionprofileservices.hpp"
#include "class.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwrender/esm4npcanimation.hpp"
#include "../mwrender/looseobjectscenebinding.hpp"
#include "../mwrender/objects.hpp"
#include "../mwrender/vismask.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwphysics/collisiontype.hpp"
#include <components/esm4/dropplacement.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/bipedmodel.hpp>
#include <components/esm4/crimerules.hpp>
#include "../mwclass/esm4base.hpp"
#include <variant>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadfact.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/misc/convert.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/resource/niffilemanager.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/nifbullet/actorragdollphysics.hpp>
#include <components/nifbullet/ragdollbonepose.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>
#include <bit>
#include <cmath>
#include <set>
#include <stdexcept>

namespace MWWorld
{
    namespace
    {
        const void* looseEquipmentBase(const ConstPtr& ptr)
        {
            if (ptr.isEmpty()) return nullptr;
            if (ptr.getType() == ESM::REC_WEAP4) return ptr.get<ESM4::Weapon>()->mBase;
            if (ptr.getType() == ESM::REC_ARMO4)
            {
                const auto* armor = ptr.get<ESM4::Armor>()->mBase;
                if (armor && (armor->mArmorFlags & ESM4::Armor::TES4_Shield)) return armor;
            }
            return nullptr;
        }

        std::optional<ESM::FormKey> looseEquipmentBaseKey(const ESMStore& store, const ConstPtr& ptr)
        {
            if (!looseEquipmentBase(ptr)) return std::nullopt;
            if (ptr.getType() == ESM::REC_WEAP4)
            {
                const auto* base = ptr.get<ESM4::Weapon>()->mBase;
                return store.get<ESM4::Weapon>().search(base->mId) == base
                    ? store.get<ESM4::Weapon>().findFormKey(base->mId) : std::nullopt;
            }
            const auto* base = ptr.get<ESM4::Armor>()->mBase;
            return store.get<ESM4::Armor>().search(base->mId) == base
                ? store.get<ESM4::Armor>().findFormKey(base->mId) : std::nullopt;
        }
    }

    void World::validateOblivionLooseWeaponState(const ESM4::RuntimeState& state) const
    {
        const ESM::FormKeyResolver resolver(mContentFiles);
        std::map<ESM::FormKey,const ESM4::RuntimeReferenceState*> references;
        for (const auto& reference : state.mReferences) references.emplace(reference.mKey,&reference);
        for (const auto& [key, snapshot] : state.mNativeLooseWeapons)
        {
            const auto* weapon = mStore.get<ESM4::Weapon>().search(snapshot.mBase);
            const auto* armor = mStore.get<ESM4::Armor>().search(snapshot.mBase);
            if (weapon && armor) throw std::runtime_error("loose equipment base has ambiguous native types");
            const ESM::Path* selected = nullptr;
            if (weapon) selected = &weapon->mModel;
            else if (armor && (armor->mArmorFlags & ESM4::Armor::TES4_Shield))
            {
                const auto found = references.find(key);
                if (found == references.end() || found->second->mBase != snapshot.mBase)
                    throw std::runtime_error("loose shield snapshot has no matching native reference");
                const auto& reference = *found->second;
                std::array<ESM4::OwnershipLayer,3> layers{};
                layers[0].mOwner = reference.mOwner.value_or(ESM::FormKey{});
                if (const auto* placed = mStore.get<ESM4::Reference>().search(key);
                    layers[0].mOwner.isNull() && placed && !placed->mDoor.destDoor.isZeroOrUnset())
                {
                    const auto* destination = mStore.get<ESM4::Reference>().search(placed->mDoor.destDoor);
                    if (!destination) throw std::runtime_error("loose shield teleport destination is unresolved");
                    if (const auto saved = references.find(destination->mFormKey); saved != references.end())
                        layers[1].mOwner = saved->second->mOwner.value_or(ESM::FormKey{});
                    else layers[1].mOwner = resolver.toFormKey(destination->mOwner);
                }
                if (layers[0].mOwner.isNull() && layers[1].mOwner.isNull())
                {
                    const auto* cell = mStore.get<ESM4::Cell>().search(reference.mCell);
                    if (!cell) throw std::runtime_error("loose shield CELL owner is unresolved");
                    layers[2].mOwner = resolver.toFormKey(cell->mOwner);
                }
                const auto owner = ESM4::resolveOwnership(ESM4::OwnershipReferenceKind::Other,layers).mOwner;
                if (!owner.isNull())
                {
                    const auto ownerId = resolver.toFormId(owner);
                    if (!ownerId || !mStore.hasEsm4ContentRecord(ESM::RefId(*ownerId)))
                        throw std::runtime_error("loose shield owner is unresolved");
                }
                const auto* npc = mStore.get<ESM4::Npc>().search(owner);
                selected = &ESM4::bipedModelForSex(armor->mModelMaleWorld,armor->mModelFemaleWorld,
                    npc && (npc->mBaseConfig.tes4.flags & ESM4::Npc::TES4_Female));
            }
            if (!selected || selected->empty()
                || MWClass::ESM4Impl::isMarkerModel(selected->getNormalized().value())
                || MWClass::ESM4Impl::isLodModel(selected->getNormalized().value())
                || Misc::ResourceHelpers::correctMeshPath(selected->getNormalized()).value() != snapshot.mModel)
                throw std::runtime_error("native loose equipment snapshot does not match its winning base/world model");
        }
    }

    void World::captureOblivionLooseWeaponState(ESM4::RuntimeState& state) const
    {
        state.mNativeLooseWeapons = mOblivionLooseWeaponStates ? *mOblivionLooseWeaponStates : NativeLooseWeaponStates{};
        if (!mPhysics) return;
        for (const auto& ptr : mPhysics->looseObjectOwners())
        {
            const auto binding = mPhysics->captureLooseObjectSceneBinding(ptr);
            // Generic low-level admissions have no native scene authority.
            // They remain separate from production native weapon publication.
            if (!binding) continue;
            const auto key = ptr.getCellRef().getFormKey();
            if (ptr.getCellRef().getCount() <= 0 || ptr.mRef->isDeleted())
            {
                state.mNativeLooseWeapons.erase(key);
                continue;
            }
            if (!looseEquipmentBase(ptr) || !ptr.isInCell() || key.isNull()
                || !ptr.getCellRef().getNativeReference()
                || mWorldModel.getPtr(ptr.getCellRef().getRefNum()) != ptr)
                throw std::logic_error("native loose weapon save has no live registered WEAP reference");
            const auto baseKey = looseEquipmentBaseKey(mStore,ptr);
            const auto model = ptr.getClass().getCorrectedModel(ptr);
            if (!baseKey || model.empty())
                throw std::runtime_error("native loose weapon save cannot resolve its winning base/model");
            const auto file = mResourceSystem->getNifFileManager()->get(model);
            const auto definition = NifBullet::loadWeaponReferenceRagdollDefinition(*file);
            if (!binding->matches(definition))
                throw std::runtime_error("native loose weapon save asset does not match its scene binding");
            state.mNativeLooseWeapons[key] = mPhysics->captureLooseObjectSnapshot(ptr, *baseKey, model.value());
        }
    }

    bool World::restoreOblivionLooseWeaponScene(const Ptr& ptr, MWRender::Objects& objects,
        MWPhysics::PhysicsSystem& physics, const VFS::Path::Normalized& model, const osg::Quat& rotation) const
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !looseEquipmentBase(ptr))
            return false;
        const auto key = ptr.getCellRef().getFormKey();
        const auto retained = mOblivionLooseWeaponStates;
        const ESM4::RuntimeLooseWeapon* saved = nullptr;
        if (retained)
            if (const auto found = retained->find(key); found != retained->end()) saved = &found->second;
        // Old saves have identity/extras but no recoverable physical history.
        // A declared drop starts at its saved reference pose with zero velocity.
        const bool legacyDrop = !saved && ptr.getCellRef().getNativeDropRelations().mDropper.has_value();
        if (!saved && !legacyDrop) return false;
        if (mPhysics.get() != &physics || !ptr.isInCell() || !ptr.getCellRef().getNativeReference()
            || ptr.getCellRef().getCount() <= 0 || !ptr.getRefData().isEnabled()
            || ptr.getRefData().getBaseNode() || key.isNull()
            || mWorldModel.getPtr(ptr.getCellRef().getRefNum()) != ptr)
            throw std::invalid_argument("native loose weapon load requires its current registered reference and physics");
        const auto* base = looseEquipmentBase(ptr);
        const auto baseKey = looseEquipmentBaseKey(mStore,ptr);
        if (!baseKey
            || model.empty() || ptr.getClass().getCorrectedModel(ptr) != model)
            throw std::invalid_argument("native loose weapon load cannot resolve its winning base/model");
        const auto position = ptr.getRefData().getPosition();
        const float scale = ptr.getCellRef().getScale();
        const std::weak_ptr<const char> identity = mOblivionRestoreIdentity;
        const auto generation = mOblivionClearGeneration;
        const auto revision = mWorldModel.getPtrRegistryRevision();
        const auto number = ptr.getCellRef().getRefNum();
        const auto referenceValid = [this, identity, generation, number, key, ptr] {
            if (identity.expired() || mOblivionClearGeneration != generation) return false;
            const auto current = mWorldModel.getPtr(number);
            return !current.isEmpty() && current == ptr && current.getCellRef().getFormKey() == key;
        };
        const auto current = [&] {
            // Check World/cache/registry ownership before touching a borrowed
            // reference that a scene callback might have retired.
            return !identity.expired() && mOblivionClearGeneration == generation
                && mOblivionLooseWeaponStates == retained && mPhysics.get() == &physics
                && mWorldModel.getPtrRegistryRevision() == revision
                && referenceValid()
                && looseEquipmentBase(ptr) == base && looseEquipmentBaseKey(mStore,ptr) == baseKey
                && ptr.getClass().getCorrectedModel(ptr) == model;
        };
        const auto file = mResourceSystem->getNifFileManager()->get(model);
        const auto definition = NifBullet::loadWeaponReferenceRagdollDefinition(*file);
        if (!current()) throw std::runtime_error("native loose weapon source changed during load");
        auto view = objects.prepareModel(ptr, std::string(model.value()), rotation, MWRender::Mask_Object,
            &definition, referenceValid);
        if (!current()) throw std::runtime_error("native loose weapon source changed during model preparation");
        std::unique_ptr<MWPhysics::PreparedLooseObject> body;
        if (saved)
            body = physics.prepareLooseObjectSnapshot(ptr, definition, *saved, *baseKey, model.value(),
                MWPhysics::CollisionType_World | MWPhysics::CollisionType_LooseObject,
                MWPhysics::CollisionType_AnyPhysical, view->looseSceneBinding());
        else
        {
            const auto scaled = NifBullet::ragdollDefinitionWithNativeScaledProperties(definition, scale);
            const auto root = osg::Matrixf::rotate(rotation) * osg::Matrixf::translate(position.asVec3());
            std::vector<NifBullet::RagdollBoneWorldPose> bones;
            for (const auto& item : scaled.mBodies)
            {
                auto bind = item.mBoneBind;
                bind.setTrans(bind.getTrans() * scale);
                bones.push_back({item.mNodeRecord, bind * root});
            }
            const auto poses = NifBullet::ragdollBodyWorldPoses(scaled, bones);
            body = physics.prepareLooseObject(ptr, scaled, NifBullet::RagdollNativeLengthScale, poses,
                MWPhysics::CollisionType_World | MWPhysics::CollisionType_LooseObject,
                MWPhysics::CollisionType_AnyPhysical, view->looseSceneBinding());
        }
        bool modelPublished = false, bodyPublished = false;
        try
        {
            if (!current() || !objects.validatePreparedModel(*view) || !physics.validatePreparedLooseObject(*body)
                || !objects.commitModel(*view))
                throw std::runtime_error("native loose weapon scene admission changed before publication");
            modelPublished = true;
            if (!current() || !objects.validatePublishedModel(*view) || !physics.commitLooseObject(*body))
                throw std::runtime_error("native loose weapon body admission changed before publication");
            bodyPublished = true;
            if (!current() || !objects.validatePublishedModel(*view) || !physics.hasLooseObject(ptr))
                throw std::runtime_error("native loose weapon ownership changed during publication");
            return true;
        }
        catch (...)
        {
            if (bodyPublished && body->hasLiveOwner()) physics.removeLooseObject(ptr);
            if (modelPublished && view->hasLiveOwner()) objects.rollbackModelAdmission(*view);
            throw;
        }
    }

    void synchronizeLooseWeaponPoses(World& world, MWPhysics::PhysicsSystem& physics)
    {
        if (world.getGameProfile() != ESM::GameProfile::Oblivion) return;
        if (world.getRayCasting() != &physics)
            throw std::invalid_argument("loose weapon synchronization requires the current World physics owner");
        struct Pose
        {
            Ptr mPtr;
            NifBullet::RagdollBonePoseWriteback mFrame;
            osg::Matrixf mRoot;
            ESM::Position mPosition;
            std::shared_ptr<const MWRender::LooseObjectSceneBinding> mBinding;
            osg::ref_ptr<MWRender::ObjectAnimation> mAnimation;
        };
        std::vector<Pose> poses;
        const auto owners = physics.looseObjectOwners();
        poses.reserve(owners.size());
        // Validate every borrowed reference and captured rigid root before
        // publishing a prefix of a frame. Capture waits for physics workers.
        for (const auto& ptr : owners)
        {
            if (!looseEquipmentBase(ptr) || !ptr.isInCell()
                || !ptr.getCellRef().getNativeReference()
                || world.getWorldModel().getPtr(ptr.getCellRef().getRefNum()) != ptr)
                throw std::logic_error("loose weapon pose has no live native world reference");
            const auto binding = physics.captureLooseObjectSceneBinding(ptr);
            auto animation = binding ? binding->lockAnimation(ptr) : osg::ref_ptr<MWRender::ObjectAnimation>{};
            if (binding && !animation)
                throw std::logic_error("loose weapon pose has no current published scene binding");
            const auto* node = ptr.getRefData().getBaseNode();
            const auto initial = node
                ? osg::Matrixf::rotate(node->getAttitude()) * osg::Matrixf::translate(node->getPosition())
                : osg::Matrixf::rotate(Misc::Convert::makeOsgQuat(ptr.getRefData().getPosition()))
                    * osg::Matrixf::translate(ptr.getRefData().getPosition().asVec3());
            const auto frame = physics.captureLooseObjectRootFrame(ptr, initial);
            auto root = frame.mWorld.mRotation.toOsgMatrix();
            root.setTrans(frame.mWorld.mTranslation);
            std::array<float,9> rotation;
            for (unsigned row=0;row<3;++row)
                for (unsigned column=0;column<3;++column)
                    rotation[row*3+column]=root(column,row);
            const osg::Vec3f translation(root.getTrans());
            const auto placement=ESM4::physicalDroppedItemPlacement(rotation,
                {translation.x(),translation.y(),translation.z()});
            auto position = ptr.getRefData().getPosition();
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                if (frame.mWorldChangeMask & 1) position.pos[axis] = placement.mPosition.pos[axis];
                if (frame.mWorldChangeMask & 2) position.rot[axis] = placement.mPosition.rot[axis];
            }
            poses.push_back({ptr,frame,root,position,binding,std::move(animation)});
        }
        // Advance retained locals only after every reference/frame is valid,
        // before cell migration can dispatch any external observers.
        for (const auto& pose : poses)
            physics.commitLooseObjectRootFrame(pose.mPtr, pose.mFrame);
        for (const auto& pose : poses)
        {
            auto ptr=pose.mPtr;
            if (pose.mBinding && !pose.mBinding->isValidFor(ptr, true))
                throw std::logic_error("loose weapon scene binding retired before pose publication");
            if (pose.mFrame.mWorldChangeMask)
                ptr.getRefData().setPosition(pose.mPosition);
            const auto* cell=ptr.getCell()->getCell();
            if ((pose.mFrame.mWorldChangeMask & 1) && cell->isExterior())
            {
                const auto location=ESM::positionToExteriorCellLocation(
                    pose.mPosition.pos[0],pose.mPosition.pos[1],cell->getWorldSpace());
                if (location != cell->getExteriorCellLocation())
                    ptr=world.moveObject(ptr,pose.mPosition.asVec3(),false);
            }
            if (auto* node=ptr.getRefData().getBaseNode())
            {
                // Keep the physical rotation, independently of its approximate
                // Euler projection. Reference scale remains on this node.
                node->setPosition(pose.mFrame.mWorld.mTranslation);
                node->setAttitude(pose.mRoot.getRotate());
            }
        }
    }

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
            const ESM4::Armor* mArmor = nullptr;
            std::optional<MWRender::ESM4NpcAnimation::CarriedShieldPose> mShieldPose{};
            std::string mWorldModel{}, mMaleWorldModel{}, mFemaleWorldModel{};
            ESM::FormId mWorldOwner{};
            const ESM4::Npc* mWorldNpcOwner = nullptr;
            const ESM4::Faction* mWorldFactionOwner = nullptr;
            std::uint32_t mWorldOwnerFlags = 0;

            bool destinationBaseCurrent(const Ptr& reference) const
            {
                return mArmor ? reference.getType() == ESM::REC_ARMO4
                        && reference.get<ESM4::Armor>()->mBase == mArmor
                    : reference.getType() == ESM::REC_WEAP4
                        && reference.get<ESM4::Weapon>()->mBase == mWeapon;
            }

            std::optional<ESM::Position> mDefaultPlacementSource = std::nullopt;

            bool baseCurrent(const ESMStore& store) const
            {
                if (!mArmor)
                    return store.get<ESM4::Weapon>().search(mWeaponId) == mWeapon
                        && mWeapon->mModel.getOriginal() == mWeaponModel && mWeapon->mFlags == mWeaponFlags
                        && mWeapon->mData.type == mWeaponType && mWeapon->mData.health == mWeaponHealth
                        && mWeapon->mEnchantment == mEnchantment && mWeapon->mEnchantmentPoints == mEnchantmentPoints;
                // New dropped REFRs have no teleport. Direct/CELL owner fields
                // are guarded by current(); retain both roles and raw NPC sex
                // so final validation requires no model/path allocation.
                const auto* owner = store.get<ESM4::Npc>().search(ESM::RefId(mWorldOwner));
                return store.get<ESM4::Armor>().search(mWeaponId) == mArmor
                    && mArmor->mFlags == mWeaponFlags && mArmor->mArmorFlags == mWeaponType
                    && mArmor->mData.health == mWeaponHealth && mArmor->mEnchantment == mEnchantment
                    && mArmor->mEnchantmentPoints == mEnchantmentPoints
                    && mArmor->mModelMaleWorld.getOriginal() == mMaleWorldModel
                    && mArmor->mModelFemaleWorld.getOriginal() == mFemaleWorldModel
                    && owner == mWorldNpcOwner && (!owner || owner->mBaseConfig.tes4.flags == mWorldOwnerFlags)
                    && store.get<ESM4::Faction>().search(ESM::RefId(mWorldOwner)) == mWorldFactionOwner;
            }

            bool carriedCurrent(const ESMStore& store) const
            {
                if (!mShieldPose) return mAnimation->isCurrentCarriedWeaponDrop(mCarried, store);
                const auto placement = MWRender::resolveOblivionNpcDropPlacement(mActor, store);
                return placement && mAnimation->isCurrentCarriedShieldPose(*mShieldPose)
                    && mCarried.mBody.mDefinition
                    && mCarried.mBody.mDefinition->mSourceHash == mShieldPose->mAttachment.mSourceHash
                    && mCarried.mAnchor.matchesCurrentScene(*placement);
            }
        };
        World* mWorld;
        MWPhysics::PhysicsSystem* mPhysics;
        std::unique_ptr<World::PreparedOblivionDynamicReferenceKey> mKeys;
        std::vector<Entry> mEntries;
        std::unique_ptr<WorldModel::PreparedLooseWeaponAdmission> mAdmission;
        std::unique_ptr<MWBase::LuaManager::PreparedSceneAdditions> mLua;
        bool mConsumed = false;
        WorldModel::LooseWeaponPublicationHooks mHooks;

        bool current() const noexcept
        {
            if (mConsumed || !mKeys->isValid() || mWorld->getRayCasting() != mPhysics || !mLua->isValid()) return false;
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
                if (e.mDefaultPlacementSource)
                {
                    const auto& current = e.mActor.getRefData().getPosition();
                    for (unsigned axis = 0; axis < 3; ++axis)
                        if (std::bit_cast<std::uint32_t>(current.pos[axis])
                                != std::bit_cast<std::uint32_t>(e.mDefaultPlacementSource->pos[axis])
                            || std::bit_cast<std::uint32_t>(current.rot[axis])
                                != std::bit_cast<std::uint32_t>(e.mDefaultPlacementSource->rot[axis])) return false;
                }
                const auto& destination = mAdmission->references()[index].getCellRef();
                const auto* placed = destination.getNativeReference();
                const auto* expected = e.mDestination.getNativeReference();
                if (!e.destinationBaseCurrent(mAdmission->references()[index])
                    || mAdmission->references()[index].getCell() != e.mCell
                    || !placed || placed->mId != expected->mId || placed->mFormKey != expected->mFormKey
                    || placed->mBaseObj != expected->mBaseObj || placed->mBaseKey != expected->mBaseKey
                    || placed->mParent != expected->mParent || placed->mParentKey != expected->mParentKey
                    || placed->mFlags != expected->mFlags || placed->mOwner != expected->mOwner
                    || placed->mGlobal != expected->mGlobal || placed->mFactionRank != expected->mFactionRank
                    || placed->mDoor.destDoor != expected->mDoor.destDoor
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
                    || !e.baseCurrent(store)
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
                    || !e.carriedCurrent(store)) return false;
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
            if (!d.mLua->commit() || !d.mKeys->commit()) std::terminate();
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
        using Reference = std::variant<LiveCellRef<ESM4::Weapon>, LiveCellRef<ESM4::Armor>>;
        std::vector<Reference> references;
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
            const bool shield = !item.isEmpty() && item.getType() == ESM::REC_ARMO
                && item.get<ESM::Armor>()->mBase->mData.mType == ESM::Armor::Shield;
            const int slot = shield ? InventoryStore::Slot_CarriedLeft : InventoryStore::Slot_CarriedRight;
            const auto equipped = inventory.getSlot(slot);
            if (item.isEmpty() || (!shield && item.getType() != ESM::REC_WEAP)
                || equipped == inventory.end() || *equipped != item)
                throw std::invalid_argument("equipment drop requires the exact carried weapon or shield");
            const auto id = OblivionProfileServices::nativeItemId(store, item.getCellRef().getRefId());
            const auto* weapon = shield ? nullptr : store.get<ESM4::Weapon>().search(id);
            const auto* armor = shield ? store.get<ESM4::Armor>().search(id) : nullptr;
            const auto baseKey = shield ? store.get<ESM4::Armor>().findFormKey(id)
                : store.get<ESM4::Weapon>().findFormKey(id);
            const auto* npc = actor.get<ESM4::Npc>()->mBase;
            const auto* cell = store.get<ESM4::Cell>().search(actor.getCell()->getCell()->getId());
            const auto cellKey = store.get<ESM4::Cell>().findFormKey(actor.getCell()->getCell()->getId());
            std::optional<MWRender::ESM4NpcAnimation::CarriedWeaponDrop> carried;
            std::optional<MWRender::ESM4NpcAnimation::CarriedShieldPose> shieldPose;
            if (shield)
            {
                shieldPose = request.mAnimation.captureCarriedShieldPose(item);
                const auto placement = MWRender::resolveOblivionNpcDropPlacement(actor, store);
                if (shieldPose && placement)
                {
                    const auto anchor = MWRender::OblivionDroppedWeaponPose::capture(shieldPose->mDropAnchor,*placement);
                    const auto file = MWBase::Environment::get().getResourceSystem()->getNifFileManager()->get(
                        shieldPose->mAttachment.mModel);
                    if (anchor && file->mPath == shieldPose->mAttachment.mModel
                        && file->mHash == shieldPose->mAttachment.mSourceHash)
                    {
                        auto definition = std::make_shared<const NifBullet::ActorRagdollDefinition>(
                            NifBullet::loadActorRagdollDefinition(*file));
                        if (!definition->mBodies.empty())
                            carried.emplace(MWRender::ESM4NpcAnimation::CarriedWeaponDrop{
                                {shieldPose->mAttachment,std::move(definition)},*anchor,{}});
                    }
                }
            }
            else carried = request.mAnimation.captureCarriedWeaponDrop(item, store);
            if ((!weapon && !armor) || (armor && !(armor->mArmorFlags & ESM4::Armor::TES4_Shield))
                || !baseKey || !baseKey->isContent() || !npc || !cell || !cellKey
                || !cellKey->isContent() || !carried || !carried->mBody.mDefinition)
                throw std::invalid_argument("equipment drop cannot resolve its winning records or bound carried NIF");
            auto removal = std::shared_ptr<InventoryStore::PreparedEquippedWeaponRemoval>(inventory.prepareEquippedWeaponRemoval(slot));
            if (!removal || !removal->isValid())
                throw std::invalid_argument("weapon drop cannot prepare its exact source debit");
            auto notification = world.prepareOblivionEquipmentNotification([removal] { removal->notify(); });
            const CellRef& source = item.getCellRef();
            ESM4::Reference placed{};
            placed.mFormKey = data->mKeys->keys()[i]; placed.mBaseObj = armor ? armor->mId : weapon->mId; placed.mBaseKey = *baseKey;
            placed.mParent = cell->mId; placed.mParentKey = *cellKey;
            placed.mPos = carried->mAnchor.getDropPlacement().mPosition;
            if (request.mUseDefaultPlacement)
            {
                const auto placement = ESM4::defaultDroppedItemPlacement(actor.getRefData().getPosition());
                if (!placement) throw std::invalid_argument("unsupported native default drop placement");
                placed.mPos = *placement;
            }
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
            if (armor) references.emplace_back(LiveCellRef<ESM4::Armor>(placed,armor));
            else references.emplace_back(LiveCellRef<ESM4::Weapon>(placed,weapon));
            CellRef& reference = std::visit([](auto& value) -> CellRef& { return value.mRef; },references.back());
            auto condition = request.mConditionAfterWear ? request.mConditionAfterWear : source.getNativeItemCondition();
            if (!condition && source.getCharge() >= 0)
                condition = static_cast<float>(double(source.getCharge()) + source.getChargeIntRemainder());
            if (condition) reference.setNativeItemCondition(*condition);
            reference.setEnchantmentCharge(source.getEnchantmentCharge());
            ESM::FormId worldOwner;
            const ESM4::Npc* worldNpcOwner = nullptr;
            std::string worldModel, rawWorldModel;
            if (armor)
            {
                const Ptr preview(std::get_if<LiveCellRef<ESM4::Armor>>(&references.back()),actor.getCell());
                worldOwner = oblivionBipedReferenceOwner(world,preview);
                worldNpcOwner = store.get<ESM4::Npc>().search(ESM::RefId(worldOwner));
                const bool female = worldNpcOwner && (worldNpcOwner->mBaseConfig.tes4.flags & ESM4::Npc::TES4_Female);
                rawWorldModel = ESM4::bipedModelForSex(armor->mModelMaleWorld,armor->mModelFemaleWorld,female).getOriginal();
                worldModel = preview.getClass().getCorrectedModel(preview);
                if (worldModel.empty()) throw std::invalid_argument("shield drop has no native world model");
                const auto file = MWBase::Environment::get().getResourceSystem()->getNifFileManager()->get(
                    VFS::Path::Normalized(worldModel));
                carried->mReferenceDefinition = std::make_shared<const NifBullet::ActorRagdollDefinition>(
                    NifBullet::loadWeaponReferenceRagdollDefinition(*file));
                if (carried->mReferenceDefinition->mBodies.empty() || !carried->mReferenceDefinition->mReferenceRoot
                    || carried->mReferenceDefinition->mSourceHash != file->mHash)
                    throw std::invalid_argument("shield drop has no identified native world body");
            }
            else
            {
                worldModel = carried->mBody.mPose.mModel.value();
                rawWorldModel = weapon->mModel.getOriginal();
            }
            CellRef sourceAfter = source;
            if (request.mConditionAfterWear) sourceAfter.setNativeItemCondition(*request.mConditionAfterWear);
            CellRef actorAfter = actor.getCellRef();
            if (!((armor ? armor->mFlags : weapon->mFlags) & 0x400))
            {
                auto relations = actorAfter.getNativeDropRelations();
                if (!relations.mItems) relations.mItems.emplace();
                relations.mItems->insert(relations.mItems->begin(), placed.mFormKey);
                actorAfter.setNativeDropRelations(std::move(relations));
                ESM4::DropRelations dropped; dropped.mDropper = actor.getCellRef().getFormKey();
                reference.setNativeDropRelations(std::move(dropped));
            }
            rotations.push_back(Misc::Convert::makeOsgQuat(placed.mPos));
            const osg::Matrixf root = osg::Matrixf::rotate(rotations.back()) * osg::Matrixf::translate(placed.mPos.asVec3());
            const auto scaled = NifBullet::ragdollDefinitionWithNativeScaledProperties(*carried->mReferenceDefinition, placed.mScale);
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
                actor.getCellRef(), std::move(actorAfter), reference, request.mConditionAfterWear.has_value(), npc, weapon, cell,
                store.get<ESM4::Npc>().search(ESM::RefId(cell->mOwner)),
                store.get<ESM4::Faction>().search(ESM::RefId(cell->mOwner)), global, placed.mGlobal, cell->mOwner, cell->mOwnershipRank,
                {}, std::move(rawWorldModel), ESM::RefId(npc->mId), ESM::RefId(placed.mBaseObj), cell->mId,
                source.getRefId(), actor.getRefData().isEnabled(), armor ? armor->mFlags : weapon->mFlags,
                armor ? armor->mArmorFlags : weapon->mData.type, armor ? armor->mData.health : weapon->mData.health,
                armor ? armor->mEnchantment : weapon->mEnchantment,
                armor ? armor->mEnchantmentPoints : weapon->mEnchantmentPoints, std::move(removal), std::move(notification)};
            entry.mArmor = armor; entry.mShieldPose = std::move(shieldPose); entry.mWorldModel = std::move(worldModel);
            if (request.mUseDefaultPlacement) entry.mDefaultPlacementSource = actor.getRefData().getPosition();
            entry.mWorldOwner = worldOwner; entry.mWorldNpcOwner = worldNpcOwner;
            entry.mWorldFactionOwner = store.get<ESM4::Faction>().search(ESM::RefId(worldOwner));
            entry.mWorldOwnerFlags = worldNpcOwner ? worldNpcOwner->mBaseConfig.tes4.flags : 0;
            if (armor)
            {
                entry.mMaleWorldModel = armor->mModelMaleWorld.getOriginal();
                entry.mFemaleWorldModel = armor->mModelFemaleWorld.getOriginal();
            }
            for (const auto& faction : npc->mFactions) entry.mFactions.emplace_back(faction.faction, faction.rank);
            data->mEntries.push_back(std::move(entry));
        }
        std::vector<WorldModel::LooseWeaponAdmission> admissions;
        for (std::size_t i = 0; i < drops.size(); ++i)
        {
            const auto& e = data->mEntries[i];
            const auto borrowed = std::visit([](const auto& value) -> WorldModel::LooseEquipmentReference {
                return std::cref(value);
            },references[i]);
            admissions.push_back({*e.mCell, borrowed, objects, physics, e.mWorldModel,
                rotations[i], MWRender::Mask_Object, *e.mCarried.mReferenceDefinition,
                NifBullet::RagdollNativeLengthScale, poses[i],
                MWPhysics::CollisionType_World | MWPhysics::CollisionType_LooseObject,
                MWPhysics::CollisionType_AnyPhysical});
        }
        data->mAdmission = world.getWorldModel().prepareLooseWeaponAdmissions(admissions);
        // Native REFR.mId is also the engine-local RefNum. The registry
        // reservation assigns it on the private nodes, independently of the
        // stable dynamic key; guard that assigned identity from this point.
        for (std::size_t i = 0; i < data->mEntries.size(); ++i)
            data->mEntries[i].mDestination.setRefNum(data->mAdmission->references()[i].getCellRef().getRefNum());
        data->mLua = MWBase::Environment::get().getLuaManager()->prepareSceneAdditions(data->mAdmission->references());
        return std::unique_ptr<PreparedEquippedWeaponDrops>(new PreparedEquippedWeaponDrops(std::move(data)));
    }
}
