#include "nativecollisiondispatcher.hpp"

#include "nativedynamicsworld.hpp"
#include "ragdollcollisionfilter.hpp"

#include <BulletCollision/BroadphaseCollision/btOverlappingPairCache.h>

#include <algorithm>
#include <stdexcept>

namespace NifBullet
{
    NativeCollisionDispatcher::NativeCollisionDispatcher(btCollisionConfiguration* configuration)
        : btCollisionDispatcher(configuration)
    {
        setNearCallback(nearCallback);
    }

    NativeCollisionDispatcher::Binding NativeCollisionDispatcher::find(const btCollisionObject* body) const noexcept
    {
        for (const auto& owner : mOwners)
        {
            const auto found = std::find(owner.mBodies.begin(), owner.mBodies.end(), body);
            if (found != owner.mBodies.end())
                return {&owner, std::size_t(found - owner.mBodies.begin())};
        }
        return {nullptr, 0};
    }

    bool NativeCollisionDispatcher::nativeEnabled(const btCollisionObject* left, const btCollisionObject* right) const
    {
        const auto a = find(left), b = find(right);
        if (!a.mOwner || !b.mOwner || a.mOwner->mWorld != b.mOwner->mWorld) return true;
        return a.mOwner->mMasks->enabled((*a.mOwner->mWords)[a.mIndex], (*b.mOwner->mWords)[b.mIndex]);
    }

    bool NativeCollisionDispatcher::needsCollision(const btCollisionObject* left, const btCollisionObject* right)
    {
        return btCollisionDispatcher::needsCollision(left, right) && nativeEnabled(left, right);
    }

    void NativeCollisionDispatcher::nearCallback(btBroadphasePair& pair, btCollisionDispatcher& dispatcher,
        const btDispatcherInfo& info)
    {
        auto& native = static_cast<NativeCollisionDispatcher&>(dispatcher);
        const auto* left = static_cast<const btCollisionObject*>(pair.m_pProxy0->m_clientObject);
        const auto* right = static_cast<const btCollisionObject*>(pair.m_pProxy1->m_clientObject);
        if (!native.nativeEnabled(left, right))
        {
            // Keep the overlap so enabling a stationary pair recreates its
            // algorithm. Release an old denied manifold through Bullet itself.
            native.find(left).mOwner->mWorld->getPairCache()->cleanOverlappingPair(pair, &dispatcher);
            return;
        }
        btCollisionDispatcher::defaultNearCallback(pair, dispatcher, info);
    }

    void NativeCollisionDispatcher::registerOwner(const void* identity, NativeDynamicsWorld& world,
        std::span<btCollisionObject* const> bodies, const std::vector<std::uint32_t>& words,
        const RagdollCollisionFilter& masks)
    {
        if (!identity || bodies.empty() || bodies.size() != words.size()
            || std::any_of(mOwners.begin(), mOwners.end(), [&](const auto& value) { return value.mIdentity == identity; }))
            throw std::invalid_argument("invalid native collision owner");
        for (const auto& owner : mOwners)
            if (owner.mWorld == &world
                && (owner.mMasks->mLayerMasks != masks.mLayerMasks || owner.mMasks->mBoneMasks != masks.mBoneMasks))
                throw std::invalid_argument("conflicting native World collision masks");
        for (std::size_t i = 0; i < bodies.size(); ++i)
        {
            if (!bodies[i] || find(bodies[i]).mOwner
                || std::find(bodies.begin(), bodies.begin() + i, bodies[i]) != bodies.begin() + i)
                throw std::invalid_argument("ambiguous native collision body");
            masks.enabled(words[i], words[i]);
        }
        mOwners.push_back({identity, &world, {bodies.begin(), bodies.end()}, &words, &masks});
    }

    void NativeCollisionDispatcher::unregisterOwner(const void* identity) noexcept
    {
        std::erase_if(mOwners, [&](const auto& owner) { return owner.mIdentity == identity; });
    }

    void NativeCollisionDispatcher::unregisterWorld(const NativeDynamicsWorld& world) noexcept
    {
        std::erase_if(mOwners, [&](const auto& owner) { return owner.mWorld == &world; });
    }

    void NativeCollisionDispatcher::refreshOwner(const void* identity) noexcept
    {
        const auto found = std::find_if(mOwners.begin(), mOwners.end(),
            [&](const auto& owner) { return owner.mIdentity == identity; });
        if (found == mOwners.end()) return;
        auto* cache = found->mWorld->getPairCache();
        auto& pairs = cache->getOverlappingPairArray();
        for (int i = 0; i < pairs.size(); ++i)
        {
            auto& pair = pairs[i];
            const auto* a = static_cast<const btCollisionObject*>(pair.m_pProxy0->m_clientObject);
            const auto* b = static_cast<const btCollisionObject*>(pair.m_pProxy1->m_clientObject);
            if ((std::find(found->mBodies.begin(), found->mBodies.end(), a) != found->mBodies.end()
                    || std::find(found->mBodies.begin(), found->mBodies.end(), b) != found->mBodies.end())
                && !nativeEnabled(a, b))
                cache->cleanOverlappingPair(pair, this);
        }
    }
}
