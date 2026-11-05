#include "oblivionanimatedreference.hpp"
#include "worldimp.hpp"
#include "class.hpp"
#include "player.hpp"
#include "oblivionactorstats.hpp"
#include "oblivionscriptmanager.hpp"
#include "../mwmechanics/oblivioncombat.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwphysics/object.hpp"
#include "../mwphysics/collisiontype.hpp"
#include "../mwphysics/oblivionragdoll.hpp"
#include "../mwrender/animation.hpp"
#include <components/esm4/loadacti.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/runtimestate.hpp>
#include <components/esm4/trapdamage.hpp>
#include <components/misc/convert.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/nif/physics.hpp>
#include <components/nifbullet/actorragdoll.hpp>
#include <components/nifbullet/actorragdollphysics.hpp>
#include <components/resource/niffilemanager.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/detournavigator/navigator.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace MWWorld
{
    bool isOblivionAnimatedReferenceAsset(Nif::FileView file)
    {
        unsigned bodies = 0;
        for (std::size_t i = 0; i < file.numRecords(); ++i)
            if (const auto* body = dynamic_cast<const Nif::bhkRigidBody*>(file.getRecord(i)))
            {
                if (body->mInfo.mMass == 0 && body->mInfo.mMotionType == Nif::HkMotionType::Motion_Fixed)
                    continue;
                if (!std::isfinite(body->mInfo.mMass) || body->mInfo.mMass <= 0
                    || body->mInfo.mMotionType != Nif::HkMotionType::Motion_Keyframed)
                    return false;
                ++bodies;
            }
        return bodies > 1;
    }

    namespace
    {
        Nif::NiTransform placement(const Ptr& ptr)
        {
            Nif::NiTransform result = Nif::NiTransform::getIdentity();
            const auto& pos = ptr.getRefData().getPosition();
            const auto matrix = osg::Matrixf::rotate(Misc::Convert::makeOsgQuat(pos));
            for (unsigned i = 0; i < 3; ++i)
                for (unsigned j = 0; j < 3; ++j) result.mRotation.mValues[i][j] = matrix(j, i);
            result.mTranslation = pos.asVec3();
            result.mScale = ptr.getCellRef().getScale();
            return result;
        }

        ESM::FormKey baseKey(const Ptr& ptr)
        {
            const auto* ref = ptr.getCellRef().getNativeReference();
            if (!ref || ptr.getType() != ESM::REC_ACTI4 || ref->mBaseKey.isNull())
                throw std::invalid_argument("animated scenery requires a native ACTI reference");
            return ref->mBaseKey;
        }

        NifBullet::ActorRagdollDefinition definition(Nif::FileView file, float scale)
        {
            if (!isOblivionAnimatedReferenceAsset(file))
                throw std::runtime_error("saved animated reference asset is no longer eligible");
            auto result = NifBullet::loadReferenceRagdollDefinition(file);
            for (const auto& body : result.mBodies)
                if (body.mBlend || body.mBlendController || !(body.mCollisionFlags & 1))
                    throw std::runtime_error("animated reference has unsupported blend or scene dispatch");
            return NifBullet::ragdollDefinitionWithNativeScaledProperties(result, scale);
        }

        void publishPose(MWPhysics::PhysicsSystem& physics, MWRender::Animation& animation,
            const Ptr& ptr, const NifBullet::ActorRagdollDefinition& graph)
        {
            const auto states = physics.captureAnimatedReferenceBodies(ptr);
            std::vector<NifBullet::RagdollBoneWorldPose> bones;
            bones.reserve(graph.mBodies.size());
            for (const auto& body : graph.mBodies)
            {
                const auto found = std::find_if(states.begin(), states.end(),
                    [&](const auto& state) { return state.mRecord == body.mRecord; });
                if (found == states.end()) throw std::logic_error("animated reference body disappeared");
                const auto origin = found->mPose.getOrigin() / NifBullet::RagdollNativeLengthScale;
                const auto q = found->mPose.getRotation();
                const NifBullet::RagdollNativeTargetPose physical{
                    {float(origin.x()), float(origin.y()), float(origin.z())},
                    {float(q.x()), float(q.y()), float(q.z()), float(q.w())}};
                bones.push_back({body.mNodeRecord, NifBullet::ragdollBoneWorldFromNativePose(
                    NifBullet::ragdollNativeSceneTargetFromBodyPose(physical, body))});
            }
            animation.applyPhysicalPose(bones, placement(ptr));
        }

        void admit(MWPhysics::PhysicsSystem& physics, MWRender::Animation& animation, const Ptr& ptr,
            const NifBullet::ActorRagdollDefinition& graph, const ESM4::RuntimeActorRagdoll* saved)
        {
            const auto base = baseKey(ptr);
            const auto model = ptr.getClass().getCorrectedModel(ptr);
            auto renderer = animation.prepareNativePhysicalPose(graph, placement(ptr));
            const auto poses = NifBullet::ragdollBodyWorldPoses(graph, renderer->initialBones());
            std::vector<NifBullet::RagdollNativeMotionRequest> motions;
            for (const auto& body : graph.mBodies)
                motions.push_back({body.mRecord, body.mFixed
                    ? NifBullet::RagdollNativeMotion::Fixed : NifBullet::RagdollNativeMotion::Dynamic});
            if (!renderer->isValid()) throw std::runtime_error("animated reference renderer changed during preparation");
            physics.addAnimatedReferenceBodies(ptr, graph, NifBullet::RagdollNativeLengthScale, poses,
                MWPhysics::CollisionType_World | MWPhysics::CollisionType_LooseObject,
                MWPhysics::CollisionType_AnyPhysical, motions, saved, base, model.value());
            // Physics publication calls no renderer callbacks. The prepared
            // renderer remains valid and its commit only moves prepared storage.
            if (!animation.commitPhysicalPose(*renderer))
                throw std::logic_error("animated reference renderer changed during physics publication");
            publishPose(physics, animation, ptr, graph);
        }
    }

    void World::updateOblivionAnimatedReference(const Ptr& ptr)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || ptr.getType() != ESM::REC_ACTI4
            || mPhysics->hasAnimatedReferenceBodies(ptr)) return;
        auto* animation = getAnimation(ptr);
        float progress = 0;
        if (!animation || !animation->getInfo("unequip", &progress) || progress < 1.f) return;
        const auto model = ptr.getClass().getCorrectedModel(ptr);
        if (model.empty()) return;
        const auto file = mResourceSystem->getNifFileManager()->get(model);
        if (!isOblivionAnimatedReferenceAsset(*file)) return;
        const auto graph = definition(*file, ptr.getCellRef().getScale());
        const auto* old = mPhysics->getObject(ptr);
        const auto oldId = DetourNavigator::ObjectId(old);
        admit(*mPhysics, *animation, ptr, graph, nullptr);
        if (old) mNavigator->removeObject(oldId, nullptr);
        Log(Debug::Info) << "TES4 animated reference released: " << ptr.getCellRef().getFormKey().serialize()
            << " bodies=" << graph.mBodies.size();
    }

    bool World::restoreOblivionAnimatedReferenceScene(const Ptr& ptr) const
    {
        if (!mOblivionAnimatedReferenceStates) return false;
        const auto found = mOblivionAnimatedReferenceStates->find(ptr.getCellRef().getFormKey());
        if (found == mOblivionAnimatedReferenceStates->end()) return false;
        auto* animation = const_cast<World*>(this)->getAnimation(ptr);
        if (!animation) throw std::runtime_error("saved animated reference has no renderer");
        const auto file = mResourceSystem->getNifFileManager()->get(ptr.getClass().getCorrectedModel(ptr));
        const auto graph = definition(*file, ptr.getCellRef().getScale());
        admit(*mPhysics, *animation, ptr, graph, &found->second);
        return true;
    }

    void World::synchronizeOblivionAnimatedReferencePoses()
    {
        for (const auto& ptr : mPhysics->animatedReferenceOwners())
        {
            auto* animation = getAnimation(ptr);
            if (!animation) throw std::logic_error("live animated reference has no renderer");
            const auto file = mResourceSystem->getNifFileManager()->get(ptr.getClass().getCorrectedModel(ptr));
            publishPose(*mPhysics, *animation, ptr, definition(*file, ptr.getCellRef().getScale()));
        }
    }

    void World::processOblivionTrapContacts()
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionCombat || !mOblivionScriptManager) return;
        const auto steps = mPhysics->takeAnimatedReferenceContacts();
        if (steps.empty()) return; // No physics step is not a separation event.
        if (!mOblivionRuntimeState)
            mOblivionRuntimeState = std::make_unique<ESM4::RuntimeState>(captureOblivionRuntimeState());
        auto& previous = mOblivionRuntimeState->mNativeTrapContacts;
        for (const auto& step : steps)
        {
            std::set<ESM4::RuntimeTrapContact> overlaps;
            for (const auto& contact : step.mContacts)
            {
                const auto& trap = contact.mReference;
                const auto& actor = contact.mActor;
                if (trap.isEmpty() || actor.isEmpty() || trap.mRef->isDeleted() || actor.mRef->isDeleted()
                    || !trap.getRefData().isEnabled() || !actor.getRefData().isEnabled()
                    || actor.getClass().getCreatureStats(actor).isDead()) continue;
                const auto variable = [&](std::string_view name) {
                    return mOblivionScriptManager->scriptVariable(trap, name).value_or(0.0);
                };
                if (!mOblivionScriptManager->scriptVariable(trap, "fTrapDamage")) continue;
                if (variable("bTrapContinuous") != 0)
                    throw std::runtime_error("continuous animated trap damage is not implemented");
                const bool player = actor == getPlayerPtr();
                const auto actorKey = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
                const ESM4::RuntimeTrapContact key{trap.getCellRef().getFormKey(), contact.mBodyRecord, actorKey};
                if (previous.contains(key))
                {
                    overlaps.insert(key);
                    continue;
                }
                if (!contact.mTouching) continue;
                overlaps.insert(key);
                const ESM4::TrapImpactSettings settings{static_cast<float>(variable("fTrapDamage")),
                    static_cast<float>(variable("fLevelledDamage")), static_cast<float>(variable("fTrapMinVelocity"))};
                const auto resistanceValue = contact.mLayer == 16 ? 67 : 65;
                const float resistance = player ? mOblivionCombat->getPlayerValue(resistanceValue)
                    : mOblivionCombat->getNonPlayerValue(actor, resistanceValue);
                const auto level = static_cast<std::uint32_t>(std::max(0, actor.getClass().getCreatureStats(actor).getLevel()));
                const float damage = ESM4::trapImpactDamage(settings, level, contact.mIncomingVelocity.length(), resistance);
                const float pushback = static_cast<float>(variable("fTrapPushBack"));
                // Allocate the latch before Health publication, and roll it
                // back if the native transaction rejects the impact. Earlier
                // successful impacts in this step must never replay on error.
                const auto [entry, inserted] = previous.insert(key);
                bool changed = false;
                try
                {
                    if (damage > 0)
                    {
                        auto adoption = mOblivionCombat->guardLifeAdoption(actor, player ? mPlayer.get() : nullptr);
                        adoptOblivionActorLife(actor);
                        changed = mOblivionCombat->applyTrapImpact(actor, player ? mPlayer.get() : nullptr,
                            damage, {contact.mIncomingVelocity.x(), contact.mIncomingVelocity.y(), contact.mIncomingVelocity.z()},
                            pushback, actor.getClass().isEssential(actor), resolveOblivionEssentialRecoverySettings(mStore),
                            resolveOblivionPlayerDynamicBaseSettings(mStore), player && getGodModeState());
                        adoption.commit();
                    }
                }
                catch (...)
                {
                    if (inserted) previous.erase(entry);
                    throw;
                }
                if (changed)
                    Log(Debug::Info) << "TES4 trap impact: reference=" << key.mReference.serialize()
                        << " body=" << key.mBodyRecord << " actor=" << key.mActor.serialize()
                        << " speed=" << contact.mIncomingVelocity.length() << " damage=" << damage
                        << " pushback=" << pushback;
            }
            previous.swap(overlaps);
        }
    }

    void World::captureOblivionAnimatedReferenceState(ESM4::RuntimeState& state) const
    {
        state.mNativeAnimatedReferences = mOblivionAnimatedReferenceStates
            ? *mOblivionAnimatedReferenceStates : NativeAnimatedReferenceStates{};
        if (!mPhysics) return;
        for (const auto& ptr : mPhysics->animatedReferenceOwners())
        {
            const auto key = ptr.getCellRef().getFormKey();
            if (ptr.mRef->isDeleted() || ptr.getCellRef().getCount() <= 0)
            {
                state.mNativeAnimatedReferences.erase(key);
                std::erase_if(state.mNativeTrapContacts,
                    [&](const auto& contact) { return contact.mReference == key; });
                continue;
            }
            const auto base = baseKey(ptr);
            const auto model = ptr.getClass().getCorrectedModel(ptr);
            state.mNativeAnimatedReferences[key] = mPhysics->captureAnimatedReferenceSnapshot(ptr, base, model.value());
        }
    }

    void World::validateOblivionAnimatedReferenceState(const ESM4::RuntimeState& state) const
    {
        for (const auto& [key, snapshot] : state.mNativeAnimatedReferences)
        {
            const auto* base = mStore.get<ESM4::Activator>().search(snapshot.mBase);
            if (!base || (base->mFlags & ESM4::Rec_Deleted) || base->mModel.empty()
                || Misc::ResourceHelpers::correctMeshPath(base->mModel.getNormalized()).value() != snapshot.mModel)
                throw std::runtime_error("animated reference snapshot has no matching winning ACTI model");
            const auto reference = std::find_if(state.mReferences.begin(), state.mReferences.end(),
                [&](const auto& ref) { return ref.mKey == key; });
            if (reference == state.mReferences.end() || reference->mBase != snapshot.mBase)
                throw std::runtime_error("animated reference snapshot has no matching owner");
            const auto file = mResourceSystem->getNifFileManager()->get(VFS::Path::Normalized(snapshot.mModel));
            // Full snapshot/controller admission also runs against detached
            // physics before scene publication. Here validate the winning asset.
            const auto graph = definition(*file, 1.f);
            MWPhysics::restoreNativeActorRagdoll(snapshot, snapshot.mBase, snapshot.mModel, graph);
        }
    }
}
