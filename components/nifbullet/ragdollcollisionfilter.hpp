#ifndef OPENMW_COMPONENTS_NIFBULLET_RAGDOLLCOLLISIONFILTER_HPP
#define OPENMW_COMPONENTS_NIFBULLET_RAGDOLLCOLLISIONFILTER_HPP

#include <array>
#include <cstdint>
#include <stdexcept>

namespace NifBullet
{
    // Original 1.2.0416 initializer 008a83c0 and predicate 008a7f70.
    // Callers own the mask state: later native mask changes and actor system
    // group allocation are separate from this predicate. Layers 32..63 are
    // outside the verified table domain and are rejected before any lookup.
    struct RagdollCollisionFilter
    {
        std::array<std::uint32_t, 32> mLayerMasks;
        std::array<std::uint32_t, 32> mBoneMasks;

        bool enabled(std::uint32_t left, std::uint32_t right) const
        {
            const unsigned a = left & 63;
            const unsigned b = right & 63;
            if (a >= 32 || b >= 32)
                throw std::invalid_argument("unverified native collision layer");
            if (a != 29 && ((left | right) & 0x4000))
                return false;
            if (!(left & 0xffff0000u) || !(right & 0xffff0000u))
                return true;
            const bool differentGroup = ((left ^ right) & 0xffff0000u) != 0;
            const unsigned boneA = (left >> 8) & 31;
            const unsigned boneB = (right >> 8) & 31;
            if (a == 8 && b == 8 && !differentGroup)
                return (mBoneMasks[boneA] & (std::uint32_t{1} << boneB)) != 0;
            if (differentGroup)
                return (mLayerMasks[a] & (std::uint32_t{1} << b)) != 0;
            if ((left & right) & 0x8000)
            {
                const unsigned distance = boneA > boneB ? boneA - boneB : boneB - boneA;
                return (mLayerMasks[a] & (std::uint32_t{1} << b)) != 0 && distance != 1;
            }
            return false;
        }
    };

    inline constexpr RagdollCollisionFilter InitialRagdollCollisionFilter{
        {
            0xffffffffu, 0xbfdb6fffu, 0xfffb6fffu, 0xbbfb6f3fu,
            0x77db7fffu, 0x77db7fffu, 0x37da7f37u, 0xb0da7f37u,
            0xf7cb7fffu, 0xbfdb6fffu, 0xf7db6fffu, 0x39db6fffu,
            0x70f051f1u, 0xbfdb6fffu, 0xf7fb7fffu, 0x82c00001u,
            0xf7fb6f3fu, 0xbfdb6fffu, 0x82c00001u, 0xbfdb6fffu,
            0xf3fb7effu, 0x30d1500du, 0xffffffffu, 0xffffffffu,
            0x36db6f7fu, 0xb7dfe77fu, 0x37cb6777u, 0x38ca2a0fu,
            0xbffb7fffu, 0xbffb7fffu, 0x00d15535u, 0xb2dfe78fu,
        },
        {
            0x00000000u, 0x010230c0u, 0x014030c0u, 0x014030c0u,
            0x014230c0u, 0x01403000u, 0x0142791eu, 0x0141ff1eu,
            0x014df0c0u, 0x014de080u, 0x014ce080u, 0x010000c0u,
            0x010241feu, 0x0109c7feu, 0x014837c0u, 0x01482780u,
            0x01482380u, 0x01001052u, 0x01000700u, 0x0141e700u,
            0x00000000u, 0x00000000u, 0x0109c7fcu, 0x01000000u,
            0x00cffffeu, 0x00000000u, 0x00000000u, 0x00000000u,
            0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
        },
    };

    // Full original allocator 00531d80. Allocation/reuse ownership belongs to
    // the caller; this operation does not reserve a unique live group.
    inline std::uint16_t nextRagdollSystemGroup(std::uint16_t previous)
    {
        const auto next = static_cast<std::uint16_t>(std::uint32_t(previous) + 1);
        return next == 0 ? 10 : next;
    }

    struct RagdollInternalCollisionFilter
    {
        RagdollCollisionFilter mMasks = InitialRagdollCollisionFilter;
        std::uint16_t mSystemGroup = 0;
    };
}

#endif
