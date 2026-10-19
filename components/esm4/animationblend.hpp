#ifndef OPENMW_COMPONENTS_ESM4_ANIMATIONBLEND_HPP
#define OPENMW_COMPONENTS_ESM4_ANIMATIONBLEND_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ESM4
{
    struct SequenceBlendWeight
    {
        float mWeight = 1;
        float mEase = 1;
        // Stored byte, compared as signed by the native interpolator.
        std::uint8_t mPriority = 0;
    };

    // Original NiBlendInterpolator normalization, including its distinct
    // one/two/many-slot paths. Entries are occupied slots in insertion order.
    // This does not allocate slots or advance activation/retirement clocks.
    std::vector<float> normalizeSequenceBlendWeights(
        std::span<const SequenceBlendWeight> slots, float threshold = 0, bool onlyHighest = false);

    struct SequenceBlendPose
    {
        std::optional<std::array<float, 3>> mTranslation;
        // Native quaternion order is w,x,y,z.
        std::optional<std::array<float, 4>> mRotation;
        std::optional<float> mScale;
        friend bool operator==(const SequenceBlendPose&, const SequenceBlendPose&) = default;
    };

    // Original NiBlendTransformInterpolator composition of sampled tracks.
    // Missing channels stay absent; the renderer retains its current channel.
    // The single occupied slot bypasses weighting and quaternion normalization.
    SequenceBlendPose blendSequenceTransforms(
        std::span<const SequenceBlendPose> poses, std::span<const float> weights);
}

#endif
