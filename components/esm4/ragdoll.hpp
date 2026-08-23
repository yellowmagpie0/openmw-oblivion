#ifndef OPENMW_COMPONENTS_ESM4_RAGDOLL_H
#define OPENMW_COMPONENTS_ESM4_RAGDOLL_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <string_view>
#include <vector>

namespace ESM4
{
    struct RagdollTransform
    {
        std::uint32_t mBone = 0;
        std::array<float, 3> mPosition{};
        std::array<float, 3> mRotation{};
    };

    struct RagdollPose
    {
        std::uint32_t mRootMetadata = 0;
        std::array<float, 3> mRootPosition{};
        std::array<float, 3> mRootRotation{};
        std::vector<RagdollTransform> mBones;
    };

    RagdollPose loadRagdollPose(std::istream& stream, std::size_t size);
    std::string_view getTes4RagdollBoneName(std::uint32_t bone);
}

#endif
