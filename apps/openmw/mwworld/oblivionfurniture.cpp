#include "worldimp.hpp"
#include "cellstore.hpp"
#include "class.hpp"
#include "esmstore.hpp"
#include "player.hpp"
#include "ptr.hpp"
#include "refdata.hpp"
#include "worldmodel.hpp"
#include "../mwmechanics/creaturestats.hpp"
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
    }

    struct World::OblivionFurnitureResources
    {
        ESM4::RuntimePlayerFurniture mIdentity;
        osg::Vec3f mMarkerOffset;
        float mEntryYaw = 0;
        std::array<std::shared_ptr<const MWRender::Animation::NativeIdleResource>, 3> mClips;
    };

    std::shared_ptr<const World::OblivionFurnitureResources> World::prepareOblivionFurnitureResources(
        const ESM4::RuntimePlayerFurniture& seat, bool verifyHashes) const
    {
        if (seat.mPhase < 2 || seat.mPhase > 4) throw std::invalid_argument("Invalid Player furniture phase");
        const auto* base = mStore.search<ESM4::Furniture>(seat.mBase);
        if (!base || (base->mFlags & ESM4::Rec_Deleted) || !(base->mActiveMarkerFlags & 0x40000000u)
            || (base->mActiveMarkerFlags & 0x80000000u) || seat.mMarker >= 30
            || !(base->mActiveMarkerFlags & (1u << seat.mMarker)))
            throw std::invalid_argument("Player furniture requires a winning active sit marker");
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
        const bool forward = marker.mPositionRef == 13 && marker.mOrientation == 0;
        const bool left = marker.mPositionRef == 11 && marker.mOrientation == 1570;
        const bool right = marker.mPositionRef == 12 && marker.mOrientation == 4712;
        // Admit only the original forward, left and right marker/clip families verified
        // with ordinary Player activation. Other directions remain unsupported.
        if ((!forward && !left && !right)
            || seat.mPlacement.rot[0] != 0 || seat.mPlacement.rot[1] != 0)
            throw std::invalid_argument("Unsupported Player furniture entry marker");
        auto result = std::make_shared<OblivionFurnitureResources>();
        result->mIdentity = seat;
        result->mIdentity.mModelHash = hexHash(view.getHash());
        result->mMarkerOffset = marker.mOffset;
        result->mEntryYaw = left ? osg::PI_2f : right ? -osg::PI_2f : 0;
        const auto& clips = forward ? ForwardClips : left ? LeftClips : RightClips;
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

    std::shared_ptr<const World::OblivionFurnitureResources> World::prepareOblivionFurnitureState(
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

    bool World::activateOblivionFurniture(const Ptr& furniture, const Ptr& actor)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || actor.isEmpty() || actor != getPlayerPtr()
            || getOblivionPlayerSitting() || furniture.isEmpty() || furniture.getType() != ESM::REC_FURN4
            || !furniture.isInCell() || actor.getCell() != furniture.getCell()
            || !furniture.getRefData().isEnabled() || furniture.mRef->isDeleted()
            || actor.getClass().getCreatureStats(actor).isDead()) return false;
        const auto* physics = mPhysics->getActor(actor);
        if (!physics || physics->isMovementSuspended() || physics->isCollisionSuspended()) return false;
        const auto authored = getOblivionReferenceBaseKey(ESM::FormKey::dynamic("player", 1));
        const auto* playerBase = authored ? mStore.search<ESM4::Npc>(*authored) : nullptr;
        if (!playerBase || !playerBase->mIsTES4) return false;
        ESM4::RuntimePlayerFurniture seat;
        seat.mReference = furniture.getCellRef().getFormKey();
        seat.mBase = ESM::FormKeyResolver(mContentFiles).toFormKey(furniture.get<ESM4::Furniture>()->mBase->mId);
        seat.mCell = furniture.getCell()->getCell()->getEsm4().mFormKey;
        seat.mPlacement = furniture.getRefData().getPosition(); seat.mScale = furniture.getCellRef().getScale();
        seat.mActorModel = playerBase->mModel.getOriginal();
        std::shared_ptr<const OblivionFurnitureResources> resources;
        float distance = std::numeric_limits<float>::max();
        for (unsigned i = 0; i < 30; ++i)
        {
            if (!(furniture.get<ESM4::Furniture>()->mBase->mActiveMarkerFlags & (1u << i))) continue;
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
        if (!resources) return false;
        if (!mOblivionRuntimeState) mOblivionRuntimeState = std::make_unique<ESM4::RuntimeState>();
        mOblivionRuntimeState->mVersion = ESM4::CurrentRuntimeStateVersion;
        mOblivionRuntimeState->mNativePlayerFurniture = resources->mIdentity;
        mOblivionFurnitureResources = std::move(resources);
        Log(Debug::Info) << "Native Player furniture: phase=2 ref=" << seat.mReference.serialize();
        return true;
    }

    bool World::rotateOblivionFurnitureView(float yaw)
    {
        if (!getOblivionPlayerSitting()) return false;
        auto& seat = *mOblivionRuntimeState->mNativePlayerFurniture;
        seat.mLookYaw = std::remainder(seat.mLookYaw + yaw, 2 * osg::PIf);
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
        auto span = [&](unsigned i) { const auto& m = resources.mClips[i]->metadata(); return m.mStopTime - m.mStartTime; };
        seat.mElapsed += duration;
        bool finished = false;
        if (seat.mElapsed >= span(index))
        {
            if (seat.mPhase == 2)
            {
                seat.mPhase = 3; seat.mElapsed = std::fmod(seat.mElapsed - span(0), span(1)); index = 1;
                Log(Debug::Info) << "Native Player furniture: phase=3 ref=" << seat.mReference.serialize();
            }
            else if (seat.mPhase == 3) seat.mElapsed = std::fmod(seat.mElapsed, span(1));
            else { seat.mElapsed = span(2); finished = true; }
        }
        const auto& clip = resources.mClips[index];
        const float frame = clip->metadata().mStartTime + seat.mElapsed;
        const auto root = index == 1 ? resources.mClips[0]->rootTranslation(resources.mClips[0]->metadata().mStopTime)
                                     : clip->rootTranslation(frame);
        if (!root) throw std::runtime_error("Player furniture lost its retained root sample");
        const auto rotation = Misc::Convert::makeOsgQuat(seat.mPlacement);
        const osg::Quat entryRotation(resources.mEntryYaw, osg::Vec3f(0, 0, -1));
        const auto position = seat.mPlacement.asVec3()
            + rotation * (resources.mMarkerOffset * seat.mScale + entryRotation * *root);
        const auto look = actor.getClass().getRotationVector(actor);
        seat.mLookYaw = std::remainder(seat.mLookYaw + look.z(), 2 * osg::PIf);
        mRendering->getCamera()->setNativeFurnitureYaw(-seat.mLookYaw);
        mPhysics->setActorMovementSuspended(actor, true);
        moveObject(actor, position);
        const auto pitch = actor.getRefData().getPosition().rot[0] + look.x();
        const float entryYaw = index == 1 ? 0 : resources.mEntryYaw;
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
        auto bind = [&](MWRender::Animation& view) {
            auto source = view.prepareNativeIdleSource(*clip);
            if (!source) throw std::runtime_error("Furniture clip cannot bind to Player skeleton");
            std::string group = source->group();
            if (!source->commit()) throw std::runtime_error("Furniture source changed during admission");
            return MWRender::Animation::NativeSequenceLayer{.mGroup = std::move(group), .mTime = frame,
                .mAccumulateRootMotion = index != 1};
        };
        const auto body = bind(animation);
        // The authored chair clips belong to the body skeleton. Applying
        // their arm transforms to the separate first-person hand rig stretches
        // its meshes. Keep that rig's ordinary presentation and track the body
        // head for the seated eye position instead.
        if (!animation.setNativeSequenceLayersForViews(std::span(&body, 1), {}))
            throw std::runtime_error("Furniture pose could not be published");
        return true;
    }
}
