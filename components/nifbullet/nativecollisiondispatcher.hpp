#ifndef OPENMW_COMPONENTS_NIFBULLET_NATIVECOLLISIONDISPATCHER_H
#define OPENMW_COMPONENTS_NIFBULLET_NATIVECOLLISIONDISPATCHER_H

#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>

#include <cstdint>
#include <span>
#include <vector>

namespace NifBullet
{
    class ActorRagdollPhysics;
    class NativeDynamicsWorld;
    struct RagdollCollisionFilter;

    // Original8A8060 preserves collidable argument order when calling8A7F70.
    // Keep Bullet's pair order too; do not sort or symmetrize the native rule.
    // Only pairs with two configured owners in the same World use this route.
    // Unbound objects and queries retain their existing Bullet policy.
    class NativeCollisionDispatcher : public btCollisionDispatcher
    {
    public:
        explicit NativeCollisionDispatcher(btCollisionConfiguration* configuration);
        bool needsCollision(const btCollisionObject* left, const btCollisionObject* right) override;

    private:
        friend class ActorRagdollPhysics;
        friend class NativeDynamicsWorld;
        struct Owner
        {
            const void* mIdentity;
            NativeDynamicsWorld* mWorld;
            std::vector<btCollisionObject*> mBodies;
            // Borrow the stable vector object, not its swappable allocation.
            // Actor/World teardown removes the binding before these expire.
            const std::vector<std::uint32_t>* mWords;
            const RagdollCollisionFilter* mMasks;
        };
        struct Binding { const Owner* mOwner; std::size_t mIndex; };
        std::vector<Owner> mOwners;
        Binding find(const btCollisionObject* body) const noexcept;
        bool nativeEnabled(const btCollisionObject* left, const btCollisionObject* right) const;
        static void nearCallback(btBroadphasePair& pair, btCollisionDispatcher& dispatcher,
            const btDispatcherInfo& info);
        void registerOwner(const void* identity, NativeDynamicsWorld& world,
            std::span<btCollisionObject* const> bodies, const std::vector<std::uint32_t>& words,
            const RagdollCollisionFilter& masks);
        void unregisterOwner(const void* identity) noexcept;
        void unregisterWorld(const NativeDynamicsWorld& world) noexcept;
        void refreshOwner(const void* identity) noexcept;
    };
}

#endif
