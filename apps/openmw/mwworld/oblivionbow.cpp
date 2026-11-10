#include "worldimp.hpp"

#include "class.hpp"
#include "esmstore.hpp"
#include "inventorystore.hpp"
#include "oblivionactorstats.hpp"
#include "oblivionarrowrecovery.hpp"
#include <components/esm4/combatsettings.hpp>
#include <components/esm4/loadgmst.hpp>
#include "oblivioncombatdata.hpp"
#include "player.hpp"
#include "projectilemanager.hpp"
#include "scene.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwmechanics/oblivioncombat.hpp"
#include "../mwmechanics/oblivionmelee.hpp"
#include "../mwphysics/collisiontype.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwrender/animation.hpp"
#include "../mwrender/camera.hpp"
#include "../mwrender/renderingmanager.hpp"

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadammo.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/settings/values.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <osg/io_utils>

namespace MWWorld
{
    namespace
    {
        bool ordinaryArrow(const ESMStore& store, const ESM::FormKey& bowKey, const ESM::FormKey& ammoKey)
        {
            const auto* bowWinner = store.getFormKeyIndex().winner(bowKey);
            const auto* ammoWinner = store.getFormKeyIndex().winner(ammoKey);
            if ((bowWinner && (bowWinner->mDeleted || bowWinner->mRecordType != ESM4::REC_WEAP))
                || (ammoWinner && (ammoWinner->mDeleted || ammoWinner->mRecordType != ESM4::REC_AMMO))) return false;
            const auto* bow = store.get<ESM4::Weapon>().search(bowKey);
            const auto* ammo = store.get<ESM4::Ammunition>().search(ammoKey);
            return bow && ammo && !(bow->mFlags & ESM4::Rec_Deleted) && !(ammo->mFlags & ESM4::Rec_Deleted)
                && bow->mData.type == 5 && !(bow->mData.flags & 1u) && ammo->mData.mFlags == 0
                && bow->mEnchantment.isZeroOrUnset() && ammo->mEnchantment.isZeroOrUnset()
                && ammo->mAmmoEffects.empty() && !ammo->mModel.empty();
        }
    }

    bool World::releaseOblivionPlayerArrow(std::uint64_t draw, MWRender::Animation& animation)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionCombat || !mPhysics) return false;
        const auto actor = getPlayerPtr();
        const auto caster = ESM::FormKey::dynamic("player", 1);
        const auto* state = mOblivionCombat->findBowState(caster);
        if (!state || state->mActionId != draw || state->mReleaseCommitted
            || !ordinaryArrow(mStore, state->mBowBase, state->mAmmoBase))
        {
            Log(Debug::Info) << "M15 Player arrow release rejected: source/owner";
            return false;
        }
        const auto position = animation.getOblivionHeldArrowPosition();
        if (!position || !animation.hasOblivionHeldArrow())
        {
            Log(Debug::Info) << "M15 Player arrow release rejected: held position=" << bool(position)
                << " attached=" << animation.hasOblivionHeldArrow();
            return false;
        }
        if (!mOblivionRuntimeState) mOblivionRuntimeState = std::make_unique<ESM4::RuntimeState>();
        const auto serial = mOblivionRuntimeState->mNextArrowProjectile;
        if (!serial || serial == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("native arrow projectile serial exhausted");
        auto release = mOblivionCombat->prepareBowRelease(*this, actor, draw);
        const auto& launch = release->launch();
        const auto& rotation = actor.getRefData().getPosition().rot;
        osg::Vec3f direction = (osg::Quat(rotation[0], osg::Vec3f(-1, 0, 0))
            * osg::Quat(rotation[2], osg::Vec3f(0, 0, -1))) * osg::Vec3f(0, 1, 0);
        // The held arrow is offset from the camera/actor center. Parallel
        // launch rays miss a centered target by that entire shoulder offset.
        // Converge on the actual crosshair point, retaining the muzzle origin
        // and subsequent gravity/collision rather than relocating the arrow.
        if (mRendering && mRendering->getCamera()->getTrackingPtr() == actor)
        {
            const auto* camera = mRendering->getCamera();
            const auto focus = mRendering->castCameraToViewportRay(.5f, .5f, 12000.f, true);
            const osg::Vec3f aim = focus.mHit ? focus.mHitPointWorld
                : osg::Vec3f(camera->getPosition()) + camera->getOrient() * osg::Vec3f(0, 12000.f, 0);
            direction = aim - *position;
        }
        if (direction.normalize() == 0) return false;
        ESM4::ArrowProjectileState arrow;
        arrow.mCaster = caster;
        arrow.mBow = launch.mBow;
        arrow.mAmmunition = launch.mAmmunition;
        arrow.mCell = actor.getCell()->getCell()->getEsm4().mFormKey;
        arrow.mDamage = launch.mDamage;
        arrow.mGravityFactor = launch.mGravityFactor;
        const auto* ammunition = mStore.get<ESM4::Ammunition>().search(launch.mAmmunition);
        arrow.mModel = Misc::ResourceHelpers::correctMeshPath(ammunition->mModel.getNormalized()).value();
        for (std::size_t i = 0; i < 3; ++i)
        {
            arrow.mPosition[i] = (*position)[i];
            arrow.mVelocity[i] = direction[i] * launch.mSpeed;
        }
        arrow.validate();
        auto projectiles = mOblivionRuntimeState->mNativeArrowProjectiles;
        if (!projectiles.emplace(serial, arrow).second) throw std::logic_error("native arrow serial reused");
        auto& inventory = actor.getClass().getInventoryStore(actor);
        if (!mOblivionCombat->validateBowRelease(*release, inventory)
            || !mOblivionCombat->commitBowRelease(*release, inventory))
        {
            Log(Debug::Info) << "M15 Player arrow release rejected: resource validation";
            return false;
        }
        // No callbacks or allocations between the resource transaction and
        // independent flight publication. A save in notification sees both.
        mOblivionRuntimeState->mNativeArrowProjectiles.swap(projectiles);
        mOblivionRuntimeState->mNextArrowProjectile = serial + 1;
        mOblivionRuntimeState->mVersion = ESM4::CurrentRuntimeStateVersion;
        animation.detachArrow();
        Log(Debug::Info) << "M15 Player arrow released: id=" << serial << " draw=" << draw
            << " bow=" << launch.mBow.serialize() << " ammo=" << launch.mAmmunition.serialize()
            << " damage=" << arrow.mDamage << " speed=" << launch.mSpeed << " gravity=" << arrow.mGravityFactor
            << " position=" << *position << " fatigue=" << mOblivionCombat->getPlayerValue(10);
        mOblivionCombat->notifyBowRelease(*release, inventory);
        return true;
    }

    void World::updateOblivionArrowProjectiles(float duration)
    {
        if (!std::isfinite(duration) || duration < 0)
            throw std::invalid_argument("native arrow frame requires finite nonnegative duration");
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionRuntimeState || !mOblivionCombat
            || !mPhysics || !mWorldScene || !mProjectileManager) return;
        auto& projectiles = mOblivionRuntimeState->mNativeArrowProjectiles;
        const auto active = [&](const ESM::FormKey& cell) {
            return std::any_of(mWorldScene->getActiveCells().begin(), mWorldScene->getActiveCells().end(),
                [&](const auto* resident) { return resident->getCell()->isEsm4()
                    && resident->getCell()->getEsm4().mFormKey == cell; });
        };
        // Settled world items retain their own timer and identity. Pickup and
        // expiry delete that owner; neither path can regenerate the old flight.
        std::vector<ESM::FormKey> settled;
        for (const auto& reference : mOblivionRuntimeState->mReferences)
            if (readRecoveredArrow(reference)) settled.push_back(reference.mKey);
        if (!settled.empty() && duration > 0)
        {
            std::vector<const ESM4::GameSetting*> settings;
            std::set<ESM::FormId> seen;
            for (const auto& record : mStore.get<ESM4::GameSetting>())
                if (seen.insert(record.mId).second)
                    if (const auto* winner = mStore.get<ESM4::GameSetting>().search(record.mId);
                        winner && !(winner->mFlags & ESM4::Rec_Deleted)) settings.push_back(winner);
            const auto lifetime = ESM4::buildArrowLifetimeSettings(settings);
            for (const auto& key : settled)
            {
                auto& references = mOblivionRuntimeState->mReferences;
                auto owner = std::find_if(references.begin(), references.end(),
                    [&](const auto& reference) { return reference.mKey == key; });
                if (owner == references.end() || owner->mDeleted || !active(owner->mCell)) continue;
                const auto ptr = mWorldModel.getDynamicNativePtr(key);
                if (ptr.isEmpty()) continue;
                if (ptr.getCellRef().getCount() <= 0)
                {
                    owner->mDeleted = true;
                    owner->mCustomState["count"] = std::int64_t(0);
                    continue;
                }
                auto state = *readRecoveredArrow(*owner);
                const auto step = ESM4::advanceArrowLifetime(state.mLifetime, duration, lifetime);
                state.mLifetime = step.mState;
                auto candidate = *owner;
                writeRecoveredArrow(candidate, state);
                if (step.mRemove)
                {
                    candidate.mDeleted = true;
                    candidate.mCustomState["count"] = std::int64_t(0);
                }
                owner->mCustomState.swap(candidate.mCustomState);
                owner->mDeleted = candidate.mDeleted;
                if (step.mRemove)
                {
                    deleteObject(ptr);
                    Log(Debug::Info) << "M15 recovered arrow expired: ref=" << key.serialize();
                }
                else if (auto* animation = getAnimation(ptr)) animation->setAlpha(state.mLifetime.mOpacity);
            }
        }
        const auto player = getPlayerPtr();
        struct Notification
        {
            Ptr mVictim;
            std::unique_ptr<MWMechanics::OblivionCombatService::PreparedPhysicalContact> mContact;
            std::vector<MWMechanics::OblivionPhysicalConditionChange> mEquipment;
        };
        std::vector<Notification> notifications;
        notifications.reserve(projectiles.size());
        // Original interior world-info setup stores this Havok gravity;
        // convert with the native projectile scale (M15 provenance, S5/272).
        const auto gravity = ESM4::projectileHavokToWorld({0, 0, -73.57500457763672f});
        for (auto it = projectiles.begin(); duration > 0 && it != projectiles.end();)
        {
            if (!active(it->second.mCell)) { ++it; continue; }
            bool removed = false;
            float remaining = duration;
            while (remaining > 0)
            {
                if (it->second.mDistance >= it->second.mMaximumDistance)
                {
                    it = projectiles.erase(it);
                    removed = true;
                    break;
                }
                const float step = std::min(remaining, 1.f / 60);
                const auto next = ESM4::advanceArrowProjectile(it->second, step, gravity);
                const osg::Vec3f from(it->second.mPosition[0], it->second.mPosition[1], it->second.mPosition[2]);
                const osg::Vec3f to(next.mPosition[0], next.mPosition[1], next.mPosition[2]);
                const auto hit = mPhysics->castRay(from, to, {player}, {}, 0xff, MWPhysics::CollisionType_Projectile);
                if (!hit.mHit)
                {
                    it->second = next;
                    remaining -= step;
                    continue;
                }
                const auto serial = it->first;
                const auto target = hit.mHitObject;
                float damage = 0;
                if (!target.isEmpty() && target.getClass().isActor())
                {
                    const auto* life = mOblivionCombat->findActorLife(target.getCellRef().getFormKey());
                    const auto* casterLife = mOblivionCombat->findActorLife(it->second.mCaster);
                    if (life && life->mPhase == ESM4::ActorLifePhase::Alive && casterLife
                        && casterLife->mPhase == ESM4::ActorLifePhase::Alive)
                    {
                        const auto resolved = MWMechanics::resolveOblivionArrowContact(*this, player, target,
                            it->second.mDamage, MWMechanics::oblivionNormalizedDifficulty(Settings::game().mDifficulty));
                        if (!resolved) throw std::runtime_error("native arrow mitigation is unsupported");
                        MWMechanics::OblivionPhysicalContactDeltas deltas;
                        deltas.mVictimHealth = -resolved->mHealth;
                        deltas.mVictimBlockFatigue = -resolved->mBlockFatigueDebit;
                        deltas.mConditionChanges = resolved->mVictimConditionChanges;
                        deltas.mRandomTransition = resolved->mRandomTransition;
                        const auto contact = mOblivionCombat->allocateAction(it->second.mCaster);
                        try
                        {
                            auto prepared = mOblivionCombat->preparePhysicalContact(contact, player, target, deltas,
                                mPlayer.get(), target.getClass().isEssential(target), resolveOblivionEssentialRecoverySettings(mStore),
                                resolveOblivionPlayerDynamicBaseSettings(mStore), getGodModeState());
                            if (!prepared || !prepared->commit()) throw std::runtime_error("stale native arrow contact");
                            notifications.push_back({target, std::move(prepared), std::move(deltas.mConditionChanges)});
                        }
                        catch (...)
                        {
                            mOblivionCombat->consumeAction(contact, it->second.mCaster);
                            throw;
                        }
                        damage = resolved->mHealth;
                    }
                }
                if (!target.isEmpty() && target.getType() == ESM::REC_STAT4 && mRendering)
                {
                    auto recovery = prepareArrowRecovery(*this, mRendering->getObjects(), *mPhysics,
                        target, it->second, serial, hit.mHitPos);
                    if (recovery)
                    {
                        const auto key = recovery->reference().mKey;
                        struct Impact
                        {
                            World* mWorld;
                            ESM4::RuntimeState* mState;
                            std::uint64_t mSerial;
                            ESM4::ArrowProjectileState mArrow;
                            std::vector<ESM4::RuntimeReferenceState> mBefore, mAfter;
                            decltype(it)* mIterator;
                        };
                        Impact impact{this, mOblivionRuntimeState.get(), serial, it->second,
                            mOblivionRuntimeState->mReferences, mOblivionRuntimeState->mReferences, &it};
                        impact.mAfter.push_back(recovery->reference());
                        std::sort(impact.mAfter.begin(), impact.mAfter.end(),
                            [](const auto& a, const auto& b) { return a.mKey < b.mKey; });
                        const auto valid = +[](void* context) noexcept {
                            const auto& data = *static_cast<Impact*>(context);
                            if (data.mWorld->mOblivionRuntimeState.get() != data.mState
                                || data.mState->mReferences != data.mBefore) return false;
                            const auto entry = data.mState->mNativeArrowProjectiles.find(data.mSerial);
                            return entry != data.mState->mNativeArrowProjectiles.end() && entry->second == data.mArrow;
                        };
                        const auto publish = +[](void* context) noexcept {
                            auto& data = *static_cast<Impact*>(context);
                            data.mState->mReferences.swap(data.mAfter);
                            *data.mIterator = data.mState->mNativeArrowProjectiles.erase(
                                data.mState->mNativeArrowProjectiles.find(data.mSerial));
                        };
                        if (recovery->commit(&impact, valid, publish).isEmpty())
                            throw std::runtime_error("native arrow recovery admission became stale");
                        removed = true;
                        Log(Debug::Info) << "M15 Player arrow recovered reference: projectile=" << serial
                            << " ref=" << key.serialize() << " surface=" << target.getCellRef().getFormKey().serialize();
                        Log(Debug::Info) << "M15 Player arrow impact: id=" << serial
                            << " target=" << target.getCellRef().getFormKey().serialize() << " damage=0";
                        break;
                    }
                }
                // An impact cannot be replayed by callbacks or a later frame.
                it = projectiles.erase(it);
                removed = true;
                Log(Debug::Info) << "M15 Player arrow impact: id=" << serial
                    << " target=" << (target.isEmpty() ? ESM::FormKey{} : target.getCellRef().getFormKey()).serialize()
                    << " damage=" << damage;
                break;
            }
            if (!removed) ++it;
        }
        // Native equipment/combat observers can save or change the scene. All
        // impact owners have been consumed before any such callback runs.
        for (auto& notification : notifications)
        {
            for (const auto& item : notification.mEquipment)
                notification.mContact->notifyEquipmentChange(item.mItem);
            const auto& target = notification.mVictim;
            const auto key = target.getCellRef().getFormKey();
            const auto* life = mOblivionCombat->findActorLife(key);
            if (life && life->mPhase == ESM4::ActorLifePhase::Alive
                && target.getRefData().isEnabled() && target.getCellRef().getCount() > 0
                && mOblivionCombat->getNonPlayerIntegerValue(target, 34) > 0
                && !isOblivionCombatTarget(key, ESM::FormKey::dynamic("player", 1)).value_or(false))
                executeOblivionStartCombat(target, player);
            MWBase::Environment::get().getWindowManager()->setEnemy(target);
        }
        std::map<std::uint64_t, ESM4::ArrowProjectileState> visible;
        for (const auto& [id, projectile] : projectiles)
            if (active(projectile.mCell)) visible.emplace(id, projectile);
        mProjectileManager->syncOblivionArrowProjectiles(visible);
    }

    void World::validateOblivionArrowProjectiles(const ESM4::RuntimeState& state) const
    {
        std::set<std::uint64_t> recovered;
        for (const auto& reference : state.mReferences)
            if (const auto arrow = readRecoveredArrow(reference))
            {
                validateRecoveredArrowSource(mStore, reference, *arrow);
                if (!recovered.insert(arrow->mProjectile).second || arrow->mProjectile >= state.mNextArrowProjectile
                    || state.mNativeArrowProjectiles.contains(arrow->mProjectile))
                    throw std::invalid_argument("duplicate or still-flying recovered arrow owner");
            }
        for (const auto& [id, arrow] : state.mNativeArrowProjectiles)
        {
            if (arrow.mCaster != ESM::FormKey::dynamic("player", 1)
                || !ordinaryArrow(mStore, arrow.mBow, arrow.mAmmunition)
                || !mStore.get<ESM4::Cell>().search(arrow.mCell))
                throw std::invalid_argument("saved native arrow source no longer resolves");
            const auto* ammo = mStore.get<ESM4::Ammunition>().search(arrow.mAmmunition);
            if (Misc::ResourceHelpers::correctMeshPath(ammo->mModel.getNormalized()).value() != arrow.mModel)
                throw std::invalid_argument("saved native arrow model no longer resolves");
        }
    }
}
