#include "ragdoll.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <type_traits>

namespace
{
    template <class T>
    T read(std::istream& stream)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        T result{};
        stream.read(reinterpret_cast<char*>(&result), sizeof(result));
        if (!stream)
            throw std::runtime_error("truncated XRGD ragdoll pose");
        return result;
    }

    float finite(std::istream& stream)
    {
        const float value = read<float>(stream);
        if (!std::isfinite(value))
            throw std::runtime_error("non-finite value in XRGD ragdoll pose");
        return value;
    }
}

namespace ESM4
{
    std::string_view getTes4RagdollBoneName(std::uint32_t bone)
    {
        // The low byte is the stable Oblivion biped index; the remaining
        // three bytes are unused padding and may retain transient data. This
        // is not the skeleton traversal order: right arm precedes left leg.
        static constexpr std::array<std::string_view, 19> names = { "Bip01", "Bip01 Pelvis", "Bip01 Spine",
            "Bip01 Spine1", "Bip01 Spine2", "Bip01 L UpperArm", "Bip01 L Forearm", "Bip01 L Hand",
            "Bip01 L Thigh", "Bip01 L Calf", "Bip01 L Foot", "Bip01 R UpperArm", "Bip01 R Forearm",
            "Bip01 R Hand", "Bip01 R Thigh", "Bip01 R Calf", "Bip01 R Foot", "Bip01 Tail", "Bip01 Head" };
        const std::size_t index = bone & 0xffu;
        return index < names.size() ? names[index] : std::string_view{};
    }

    RagdollPose loadRagdollPose(std::istream& stream, std::size_t size)
    {
        constexpr std::size_t blockSize = sizeof(std::uint32_t) + 6 * sizeof(float);
        if (size < blockSize || (size - blockSize) % blockSize != 0)
            throw std::runtime_error("invalid XRGD ragdoll pose size");
        const std::size_t boneCount = (size - blockSize) / blockSize;
        if (boneCount > 512)
            throw std::runtime_error("XRGD ragdoll pose has too many bones");

        RagdollPose result;
        // TES4 prefixes the per-bone array with a root-pose block. Its first
        // word contains transient Havok metadata (the low byte commonly
        // duplicates the first bone id), not a file-format version.
        result.mRootMetadata = read<std::uint32_t>(stream);
        for (float& value : result.mRootPosition)
            value = finite(stream);
        for (float& value : result.mRootRotation)
            value = finite(stream);
        result.mBones.reserve(boneCount);
        for (std::size_t i = 0; i < boneCount; ++i)
        {
            RagdollTransform bone;
            bone.mBone = read<std::uint32_t>(stream);
            for (float& value : bone.mPosition)
                value = read<float>(stream);
            for (float& value : bone.mRotation)
                value = read<float>(stream);

            // The six base-game dead actor references at the sewer exit use
            // quiet NaNs to mark individual Havok bones as unavailable. Keep
            // consuming their fixed-size blocks but never expose those values
            // to the scene graph as transforms.
            if (std::ranges::all_of(bone.mPosition, [](float value) { return std::isfinite(value); })
                && std::ranges::all_of(bone.mRotation, [](float value) { return std::isfinite(value); }))
                result.mBones.push_back(bone);
        }
        return result;
    }
}
