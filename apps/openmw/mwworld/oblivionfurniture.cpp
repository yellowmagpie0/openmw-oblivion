#include "worldimp.hpp"
#include "cellstore.hpp"
#include "class.hpp"
#include "esmstore.hpp"
#include "player.hpp"
#include "ptr.hpp"
#include "refdata.hpp"
#include "worldmodel.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/oblivionai.hpp"
#include "../mwrender/esm4npcanimation.hpp"
#include <components/esm4/loadpack.hpp>
#include "../mwphysics/actor.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwrender/animation.hpp"
#include "../mwrender/camera.hpp"
#include "../mwrender/renderingmanager.hpp"
#include <components/debug/debuglog.hpp>
#include <components/esm4/loadfurn.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrefr.hpp>
#include <limits>
#include <components/esm4/runtimestate.hpp>
#include <components/misc/convert.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/nif/extra.hpp>
#include <components/resource/niffilemanager.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/sceneutil/keyframe.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace MWWorld
{
    namespace
    {
        constexpr std::array<std::string_view, 3> ForwardClips{
            "chair_forwardenter.kf", "dynamicidle_sit.kf", "chair_forwardexit.kf"};
        constexpr std::array<std::string_view, 3> LeftClips{
            "chair_leftenter.kf", "dynamicidle_sit.kf", "chair_leftexit.kf"};
        constexpr std::array<std::string_view, 3> RightClips{
            "chair_rightenter.kf", "dynamicidle_sit.kf", "chair_rightexit.kf"};
        constexpr std::array<std::string_view, 3> BackClips{
            "chair_backenter.kf", "dynamicidle_sit.kf", "chair_backexit.kf"};
        constexpr std::array<std::string_view, 3> BedLeftClips{
            "bedleft_enter.kf", "dynamicidle_sleep.kf", "bedleft_exit.kf"};
        constexpr std::array<std::string_view, 3> BedRightClips{
            "bedright_enter.kf", "dynamicidle_sleep.kf", "bedright_exit.kf"};
        std::string hexHash(std::string_view bytes)
        {
            constexpr char digits[] = "0123456789abcdef";
            std::string out;
            for (unsigned char c : bytes) { out += digits[c >> 4]; out += digits[c & 15]; }
            return out;
        }
        bool samePlacement(const ESM::Position& a, const ESM::Position& b)
        {
            for (unsigned i = 0; i < 3; ++i)
                if (a.pos[i] != b.pos[i] || a.rot[i] != b.rot[i]) return false;
            return true;
        }

        // A Travel package can name the chair itself as the exact destination.
        // Nearby-radius destinations only require the actor to be in the area.
        bool usesLocationChair(ESM4::AIPackageType type)
        {
            return type == ESM4::AIPackageType::UseItemAt || type == ESM4::AIPackageType::Travel;
        }

        const ESM4::Reference* packageChair(const ESMStore& store, const ESM::FormKey& key)
        {
            const auto* pack = store.search<ESM4::AIPackage>(key);
            if (!pack || pack->mLocationData.mKind != ESM4::PackageLocationKind::NearReference
                || pack->mLocationData.mRadius != 0)
                return nullptr;
            const auto* ref = store.search<ESM4::Reference>(pack->mLocationData.mReferenceKey);
            const auto* chair = ref ? store.search<ESM4::Furniture>(ref->mBaseKey) : nullptr;
            const auto flag = pack->mPackageType == ESM4::AIPackageType::Sleep ? 0x80000000u : 0x40000000u;
            return chair && (chair->mActiveMarkerFlags & flag) ? ref : nullptr;
        }
    }

    struct OblivionFurnitureResources
    {
        ESM4::RuntimePlayerFurniture mIdentity;
        osg::Vec3f mMarkerOffset;
        float mEntryYaw = 0;
        float mIdleYaw = 0;
        std::array<std::shared_ptr<const MWRender::Animation::NativeIdleResource>, 3> mClips;

        mutable std::array<std::string, 3> mBoundGroups;

        float span(unsigned i) const
        {
            const auto& m = mClips[i]->metadata();
            return m.mStopTime - m.mStartTime;
        }

        osg::Vec3f position(unsigned index, float frame) const
        {
            const auto root = index == 1 ? mClips[0]->rootTranslation(mClips[0]->metadata().mStopTime)
                                         : mClips[index]->rootTranslation(frame);
            if (!root) throw std::runtime_error("Furniture lost its retained root sample");
            const osg::Quat entryRotation(mEntryYaw, osg::Vec3f(0, 0, -1));
            return mIdentity.mPlacement.asVec3() + Misc::Convert::makeOsgQuat(mIdentity.mPlacement)
                * (mMarkerOffset * mIdentity.mScale + entryRotation * *root);
        }

        bool bind(MWRender::Animation& view, unsigned index, float frame, bool player) const
        {
            auto& group = mBoundGroups[index];
            if (group.empty() || !view.hasAnimation(group))
            {
                auto source = view.prepareNativeIdleSource(*mClips[index]);
                if (!source) return false;
                group = source->group();
                if (!source->commit()) return false;
            }
            const MWRender::Animation::NativeSequenceLayer body{.mGroup = group, .mTime = frame,
                .mAccumulateRootMotion = index != 1};
            // Chair clips belong to the body skeleton, including in first person.
            return player ? view.setNativeSequenceLayersForViews(std::span(&body, 1), {})
                          : view.setNativeSequenceLayers(std::span(&body, 1));
        }
    };

    // Per-rendered-actor projection of the saved package, placement and phase.
    // Releasing the scene releases the clip resources and the marker reservation.
    struct OblivionPackageSeat
    {
        std::shared_ptr<const OblivionFurnitureResources> mResources;
        unsigned mPhase = 2;
        float mElapsed = 0.f;
    };

    std::shared_ptr<const OblivionFurnitureResources> World::prepareOblivionFurnitureResources(
        const ESM4::RuntimePlayerFurniture& seat, bool verifyHashes) const
    {
        if (seat.mPhase < 2 || seat.mPhase > 4) throw std::invalid_argument("Invalid Player furniture phase");
        const auto* base = mStore.search<ESM4::Furniture>(seat.mBase);
        const auto type = base ? (base->mActiveMarkerFlags & 0xc0000000u) : 0u;
        const bool bed = type == 0x80000000u;
        // Saved furniture identities belong to the player's chair. NPC beds
        // are reconstructed from their sleep package on every scene load.
        if (!base || (base->mFlags & ESM4::Rec_Deleted)
            || (type != 0x40000000u && !(bed && !verifyHashes))
            || seat.mMarker >= 30
            || !(base->mActiveMarkerFlags & (1u << seat.mMarker)))
            throw std::invalid_argument("Furniture requires a supported active marker");
        const auto model = Misc::ResourceHelpers::correctMeshPath(base->mModel.getNormalized());
        const auto file = mResourceSystem->getNifFileManager()->get(model);
        const Nif::FileView view(*file);
        const Nif::BSFurnitureMarker* markers = nullptr;
        for (std::size_t i = 0; i < view.numRecords(); ++i)
            if (const auto* found = dynamic_cast<const Nif::BSFurnitureMarker*>(view.getRecord(i)))
            {
                if (markers) throw std::invalid_argument("Ambiguous furniture marker bank");
                markers = found;
            }
        if (!markers || seat.mMarker >= markers->mLegacyMarkers.size())
            throw std::invalid_argument("Missing Player furniture marker");
        const auto& marker = markers->mLegacyMarkers[seat.mMarker];
        // PositionRef selects the entrance clip. The NIF stores its orientation
        // separately in thousandths of a radian. A rotated stool uses the same
        // forward entry as a chair with a different local heading.
        const bool forward = !bed && marker.mPositionRef == 13;
        const bool left = marker.mPositionRef == (bed ? 1 : 11);
        const bool right = marker.mPositionRef == (bed ? 2 : 12);
        const bool back = !bed && marker.mPositionRef == 14;
        if ((!forward && !left && !right && !back) || marker.mOrientation > 6284
            || seat.mPlacement.rot[0] != 0 || seat.mPlacement.rot[1] != 0)
            throw std::invalid_argument("Unsupported chair entry marker");
        auto result = std::make_shared<OblivionFurnitureResources>();
        result->mIdentity = seat;
        result->mIdentity.mModelHash = hexHash(view.getHash());
        result->mMarkerOffset = marker.mOffset;
        const float clipHeading = left ? osg::PI_2f : right ? -osg::PI_2f : back ? osg::PIf : 0.f;
        const float entry = marker.mOrientation * 0.001f;
        // Preserve authored cardinal headings exactly across save/reload.
        const float quarterTurns = std::round(entry / osg::PI_2f);
        result->mEntryYaw = std::remainder(std::abs(entry - quarterTurns * osg::PI_2f) < 0.001f
            ? quarterTurns * osg::PI_2f : entry, 2.f * osg::PIf);
        result->mIdleYaw = std::remainder(result->mEntryYaw - clipHeading, 2.f * osg::PIf);
        const auto& clips = bed ? (left ? BedLeftClips : BedRightClips)
            : forward ? ForwardClips : left ? LeftClips : right ? RightClips : BackClips;
        const auto actorModel = VFS::Path::normalizeFilename(seat.mActorModel);
        const auto slash = actorModel.rfind('/');
        if (slash == std::string::npos || actorModel.find("..") != std::string::npos)
            throw std::invalid_argument("Furniture has no native actor animation bank");
        const auto directory = actorModel.substr(0, slash + 1) + "idleanims/";
        for (std::size_t i = 0; i < clips.size(); ++i)
        {
            auto& clip = result->mClips[i];
            clip = MWRender::Animation::acquireNativeIdleResource(*mResourceSystem, directory + std::string(clips[i]));
            if (!clip || !clip->rootTranslation(clip->metadata().mStartTime)
                || !clip->rootTranslation(clip->metadata().mStopTime)
                || clip->metadata().mStopTime <= clip->metadata().mStartTime)
                throw std::invalid_argument("Furniture clip lacks its authored root motion");
            result->mIdentity.mClipHashes[i] = hexHash(clip->fileHash());
        }
        if (verifyHashes && (seat.mModelHash != result->mIdentity.mModelHash
                || seat.mClipHashes != result->mIdentity.mClipHashes))
            throw std::invalid_argument("Saved Player furniture assets changed");
        const auto& clip = result->mClips[seat.mPhase - 2]->metadata();
        if (!std::isfinite(seat.mElapsed) || seat.mElapsed < 0 || seat.mElapsed >= clip.mStopTime - clip.mStartTime)
            throw std::invalid_argument("Saved Player furniture clock is outside its clip");
        return result;
    }

    std::shared_ptr<const OblivionFurnitureResources> World::prepareOblivionFurnitureState(
        const ESM4::RuntimeState& state) const
    {
        if (!state.mNativePlayerFurniture) return {};
        const auto& seat = *state.mNativePlayerFurniture;
        const auto authored = mStore.findEsm4FormKey("Player");
        const auto* base = authored ? mStore.search<ESM4::Npc>(*authored) : nullptr;
        if (!base || !base->mIsTES4 || seat.mActorModel != base->mModel.getOriginal())
            throw std::invalid_argument("Furniture Player actor model no longer resolves");
        return prepareOblivionFurnitureResources(seat, true);
    }

    unsigned World::getOblivionPlayerSitting() const
    {
        return mOblivionRuntimeState && mOblivionRuntimeState->mNativePlayerFurniture
            ? mOblivionRuntimeState->mNativePlayerFurniture->mPhase : 0;
    }

    std::shared_ptr<const OblivionFurnitureResources> World::chooseOblivionChairMarker(
        const Ptr& furniture, const Ptr& actor, std::string_view actorModel, bool& occupied) const
    {
        ESM4::RuntimePlayerFurniture seat;
        seat.mReference = furniture.getCellRef().getFormKey();
        seat.mBase = ESM::FormKeyResolver(mContentFiles).toFormKey(furniture.get<ESM4::Furniture>()->mBase->mId);
        seat.mCell = furniture.getCell()->getCell()->getEsm4().mFormKey;
        seat.mPlacement = furniture.getRefData().getPosition(); seat.mScale = furniture.getCellRef().getScale();
        seat.mActorModel = actorModel;
        std::uint32_t reserved = 0;
        if (getOblivionPlayerSitting() && actor != mPlayer->getPlayer())
        {
            const auto& playerSeat = *mOblivionRuntimeState->mNativePlayerFurniture;
            if (playerSeat.mReference == seat.mReference)
                reserved |= 1u << playerSeat.mMarker;
        }
        furniture.getCell()->forEachType<ESM4::Npc>([&](const Ptr& other) {
            if (other != actor)
                if (const auto* view = dynamic_cast<const MWRender::ESM4NpcAnimation*>(getAnimation(ConstPtr(other)));
                    view && view->mPackageSeat)
                {
                    const auto& used = view->mPackageSeat->mResources->mIdentity;
                    if (used.mReference == seat.mReference)
                        reserved |= 1u << used.mMarker;
                }
            return true;
        });
        occupied = reserved != 0;
        const auto available = furniture.get<ESM4::Furniture>()->mBase->mActiveMarkerFlags & ~reserved;
        std::shared_ptr<const OblivionFurnitureResources> resources;
        float distance = std::numeric_limits<float>::max();
        for (std::uint8_t i = 0; i < 30; ++i)
        {
            if (!(available & (1u << i))) continue;
            seat.mMarker = i;
            try
            {
                auto candidate = prepareOblivionFurnitureResources(seat, false);
                const auto entry = seat.mPlacement.asVec3()
                    + Misc::Convert::makeOsgQuat(seat.mPlacement) * (candidate->mMarkerOffset * seat.mScale);
                const float d = (entry - actor.getRefData().getPosition().asVec3()).length2();
                if (d < distance) { distance = d; resources = std::move(candidate); }
            }
            catch (const std::exception& error)
            {
                Log(Debug::Verbose) << "Native furniture marker unavailable: " << error.what();
            }
        }
        return resources;
    }

    bool World::activateOblivionFurniture(const Ptr& furniture, const Ptr& actor)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || actor.isEmpty() || actor != getPlayerPtr()
            || getOblivionPlayerSitting() || furniture.isEmpty() || furniture.getType() != ESM::REC_FURN4
            || !furniture.isInCell() || actor.getCell() != furniture.getCell()
            || !furniture.getRefData().isEnabled() || furniture.mRef->isDeleted()
            || actor.getClass().getCreatureStats(actor).isDead()) return false;
        const auto* physics = mPhysics->getActor(actor);
        if (!physics || physics->isMovementSuspended() || physics->isCollisionSuspended()) return false;
        if ((furniture.get<ESM4::Furniture>()->mBase->mActiveMarkerFlags & 0xc0000000u) != 0x40000000u) return false;
        const auto authored = getOblivionReferenceBaseKey(ESM::FormKey::dynamic("player", 1));
        const auto* playerBase = authored ? mStore.search<ESM4::Npc>(*authored) : nullptr;
        if (!playerBase || !playerBase->mIsTES4) return false;
        bool occupied = false;
        const auto resources = chooseOblivionChairMarker(furniture, actor, playerBase->mModel.getOriginal(), occupied);
        if (!resources) return false;
        if (!mOblivionRuntimeState) mOblivionRuntimeState = std::make_unique<ESM4::RuntimeState>();
        mOblivionRuntimeState->mVersion = ESM4::CurrentRuntimeStateVersion;
        mOblivionRuntimeState->mNativePlayerFurniture = resources->mIdentity;
        mOblivionFurnitureResources = std::move(resources);
        Log(Debug::Info) << "Native Player furniture: phase=2 ref=" << furniture.getCellRef().getFormKey().serialize();
        return true;
    }

    bool World::rotateOblivionFurnitureView(float yaw, bool absolute)
    {
        if (!getOblivionPlayerSitting()) return false;
        auto& seat = *mOblivionRuntimeState->mNativePlayerFurniture;
        seat.mLookYaw = std::remainder(absolute ? yaw - getPlayerPtr().getRefData().getPosition().rot[2]
                                                : seat.mLookYaw + yaw, 2 * osg::PIf);
        mRendering->getCamera()->setNativeFurnitureYaw(-seat.mLookYaw);
        return true;
    }

    bool World::requestOblivionFurnitureExit()
    {
        if (getOblivionPlayerSitting() != 3) return false;
        auto& seat = *mOblivionRuntimeState->mNativePlayerFurniture;
        seat.mPhase = 4; seat.mElapsed = 0;
        Log(Debug::Info) << "Native Player furniture: phase=4 ref=" << seat.mReference.serialize();
        return true;
    }

    bool World::updateOblivionPlayerFurniture(MWRender::Animation& animation, const Ptr& actor, float duration,
        bool interrupted)
    {
        if (actor != getPlayerPtr() || !getOblivionPlayerSitting()) return false;
        auto& seat = *mOblivionRuntimeState->mNativePlayerFurniture;
        const auto* placed = mStore.search<ESM4::Reference>(seat.mReference);
        const auto furniture = placed ? mWorldModel.getPtr(placed->mId) : Ptr();
        bool invalid = interrupted || actor.getClass().getCreatureStats(actor).isDead()
            || furniture.isEmpty() || !furniture.isInCell() || furniture.getCell() != actor.getCell()
            || !furniture.getRefData().isEnabled() || furniture.mRef->isDeleted()
            || !samePlacement(seat.mPlacement, furniture.getRefData().getPosition())
            || seat.mScale != furniture.getCellRef().getScale();
        if (invalid)
        {
            mOblivionRuntimeState->mNativePlayerFurniture.reset(); mOblivionFurnitureResources.reset();
            if (!interrupted) animation.setNativeSequenceLayersForViews({}, {});
            mRendering->getCamera()->setNativeFurnitureYaw(0);
            mPhysics->setActorMovementSuspended(actor, false);
            Log(Debug::Info) << "Native Player furniture: cancelled";
            return false;
        }
        if (!mOblivionFurnitureResources
            || mOblivionFurnitureResources->mIdentity.mReference != seat.mReference
            || mOblivionFurnitureResources->mIdentity.mClipHashes != seat.mClipHashes)
            mOblivionFurnitureResources = prepareOblivionFurnitureResources(seat, true);
        const auto& resources = *mOblivionFurnitureResources;
        unsigned index = seat.mPhase - 2;
        seat.mElapsed += duration;
        bool finished = false;
        if (seat.mElapsed >= resources.span(index))
        {
            if (seat.mPhase == 2)
            {
                seat.mPhase = 3; seat.mElapsed = std::fmod(seat.mElapsed - resources.span(0), resources.span(1)); index = 1;
                Log(Debug::Info) << "Native Player furniture: phase=3 ref=" << seat.mReference.serialize();
            }
            else if (seat.mPhase == 3) seat.mElapsed = std::fmod(seat.mElapsed, resources.span(1));
            else { seat.mElapsed = resources.span(2); finished = true; }
        }
        const auto& clip = resources.mClips[index];
        const float frame = clip->metadata().mStartTime + seat.mElapsed;
        const auto position = resources.position(index, frame);
        const auto look = actor.getClass().getRotationVector(actor);
        seat.mLookYaw = std::remainder(seat.mLookYaw + look.z(), 2 * osg::PIf);
        mRendering->getCamera()->setNativeFurnitureYaw(-seat.mLookYaw);
        mPhysics->setActorMovementSuspended(actor, true);
        moveObject(actor, position);
        const auto pitch = actor.getRefData().getPosition().rot[0] + look.x();
        const float entryYaw = index == 1 ? resources.mIdleYaw : resources.mEntryYaw;
        rotateObject(actor, osg::Vec3f(pitch, 0, seat.mPlacement.rot[2] + entryYaw + (finished ? osg::PIf : 0)));
        mPlayer->consumeOblivionJumpRequest(); mPlayer->consumeOblivionCastRequest();
        if (finished)
        {
            mOblivionRuntimeState->mNativePlayerFurniture.reset(); mOblivionFurnitureResources.reset();
            animation.setNativeSequenceLayersForViews({}, {});
            mRendering->getCamera()->setNativeFurnitureYaw(0);
            mPhysics->setActorMovementSuspended(actor, false);
            Log(Debug::Info) << "Native Player furniture: phase=0";
            return false;
        }
        if (!resources.bind(animation, index, frame, true))
            throw std::runtime_error("Furniture pose could not be published");
        return true;
    }

    unsigned World::getOblivionSitting(const ESM::FormKey& actor, const Ptr& resident) const
    {
        if (actor == ESM::FormKey::dynamic("player", 1))
            return getOblivionPlayerSitting();
        const auto* view = resident.isEmpty() ? nullptr
            : dynamic_cast<const MWRender::ESM4NpcAnimation*>(getAnimation(ConstPtr(resident)));
        if (view && view->mPackageSeat)
        {
            const auto* base = mStore.search<ESM4::Furniture>(view->mPackageSeat->mResources->mIdentity.mBase);
            return base && (base->mActiveMarkerFlags & 0x80000000u) ? 1 : view->mPackageSeat->mPhase;
        }
        const auto* ai = mOblivionAi ? mOblivionAi->state(actor) : nullptr;
        if (!ai || (ai->mPhase != ESM4::PackagePhase::Act && ai->mPhase != ESM4::PackagePhase::Wait
            && ai->mPhase != ESM4::PackagePhase::Complete))
            return 0;
        if (ai->mPackageType == ESM4::AIPackageType::Sleep && ai->mPhase != ESM4::PackagePhase::Complete)
            return 1;
        if (!view && usesLocationChair(ai->mPackageType) && packageChair(mStore, ai->mPackage)
            && !hasOblivionCombatController(actor).value_or(false))
            return 3;
        return 0;
    }

    bool World::updateOblivionNpcFurniture(MWRender::Animation& animation, const Ptr& actor, float duration,
        bool interrupted)
    {
        if (actor.getType() != ESM::REC_NPC_4 || !mOblivionAi)
            return false;
        auto* view = dynamic_cast<MWRender::ESM4NpcAnimation*>(&animation);
        if (!view) return false;
        auto& seat = view->mPackageSeat;
        const auto* ai = mOblivionAi->state(actor);
        const bool atLocation = ai && (usesLocationChair(ai->mPackageType) || ai->mPackageType == ESM4::AIPackageType::Sleep)
            && (ai->mPhase == ESM4::PackagePhase::Arrive || ai->mPhase == ESM4::PackagePhase::Act
                || ai->mPhase == ESM4::PackagePhase::Wait || ai->mPhase == ESM4::PackagePhase::Complete);
        if (!seat && !atLocation) return false;
        const auto& stats = actor.getClass().getCreatureStats(actor);
        const bool cancelled = interrupted || stats.isDead() || stats.getAiSequence().isInCombat()
            || !actor.isInCell();
        const auto* placed = atLocation && !cancelled ? packageChair(mStore, ai->mPackage) : nullptr;
        // A native combat controller can interrupt a completed Travel while the
        // saved base package still names the chair. Play the normal exit once.
        const Ptr chair = placed && !hasOblivionCombatController(ai->mActor).value_or(false)
            ? mWorldModel.getPtr(placed->mId) : Ptr();
        const bool eligible = !chair.isEmpty() && chair.getType() == ESM::REC_FURN4
            && chair.getCell() == actor.getCell() && chair.getRefData().isEnabled() && !chair.mRef->isDeleted();
        if (!seat)
        {
            if (!eligible || (actor.getRefData().getPosition().asVec3() - chair.getRefData().getPosition().asVec3()).length2() > 128.f * 128.f
                || view->mUnsupportedPackageSeat == placed->mFormKey)
                return false;
            if (view->mPackageSeatRetry > 0.f)
            {
                view->mPackageSeatRetry -= duration;
                return false;
            }
            bool occupied = false;
            const auto resources = chooseOblivionChairMarker(chair, actor, actor.get<ESM4::Npc>()->mBase->mModel.getOriginal(), occupied);
            if (!resources)
            {
                if (occupied)
                {
                    view->mPackageSeatRetry = 1.f;
                    return false;
                }
                view->mUnsupportedPackageSeat = placed->mFormKey;
                Log(Debug::Info) << "Native NPC furniture: unsupported marker ref=" << placed->mFormKey.serialize();
                return false;
            }
            seat = std::make_shared<OblivionPackageSeat>();
            seat->mResources = resources;
            // Actors promoted while waiting are already using the chair.
            seat->mPhase = ai->mPhase == ESM4::PackagePhase::Wait || ai->mPhase == ESM4::PackagePhase::Complete
                || (ai->mPackageType == ESM4::AIPackageType::Sleep && ai->mPhase == ESM4::PackagePhase::Act) ? 3 : 2;
            actor.getClass().getCreatureStats(actor).setDrawState(MWMechanics::DrawState::Nothing);
            view->showWeapons(false);
            Log(Debug::Info) << "Native NPC furniture: actor=" << ai->mActor.serialize()
                << " ref=" << placed->mFormKey.serialize() << " phase=" << seat->mPhase;
        }
        const auto& resources = *seat->mResources;
        const auto& identity = resources.mIdentity;
        if (cancelled || identity.mCell != actor.getCell()->getCell()->getEsm4().mFormKey)
        {
            seat.reset();
            if (!interrupted) animation.setNativeSequenceLayers({});
            mPhysics->setActorMovementSuspended(actor, false);
            return false;
        }
        if ((!eligible || placed->mFormKey != identity.mReference) && seat->mPhase != 4)
        {
            seat->mElapsed = 0.f;
            seat->mPhase = 4;
        }
        unsigned index = seat->mPhase - 2;
        seat->mElapsed += duration;
        bool finished = false;
        if (seat->mElapsed >= resources.span(index))
        {
            if (index == 2) { seat->mElapsed = resources.span(2); finished = true; }
            else
            {
                if (index == 0) Log(Debug::Info) << "Native NPC furniture: actor=" << actor.getCellRef().getFormKey().serialize() << " phase=3";
                seat->mElapsed = std::fmod(seat->mElapsed - resources.span(index), resources.span(1));
                seat->mPhase = 3;
                index = 1;
            }
        }
        const float frame = resources.mClips[index]->metadata().mStartTime + seat->mElapsed;
        const auto position = resources.position(index, frame);
        const float yaw = std::remainder(identity.mPlacement.rot[2] + (index == 1 ? resources.mIdleYaw : resources.mEntryYaw)
            + (finished ? osg::PIf : 0.f), 2 * osg::PIf);
        mPhysics->setActorMovementSuspended(actor, true);
        if (actor.getRefData().getPosition().asVec3() != position) moveObject(actor, position);
        if (actor.getRefData().getPosition().rot[2] != yaw) rotateObject(actor, {0.f, 0.f, yaw});
        if (finished)
        {
            seat.reset();
            animation.setNativeSequenceLayers({});
            mPhysics->setActorMovementSuspended(actor, false);
            Log(Debug::Info) << "Native NPC furniture: actor=" << actor.getCellRef().getFormKey().serialize() << " phase=0";
            return false;
        }
        if (!resources.bind(animation, index, frame, false))
        {
            view->mUnsupportedPackageSeat = identity.mReference;
            Log(Debug::Warning) << "Native chair clip unavailable for actor " << actor.getCellRef().getFormKey().serialize();
            seat.reset();
            animation.setNativeSequenceLayers({});
            mPhysics->setActorMovementSuspended(actor, false);
            return false;
        }
        return true;
    }
}
