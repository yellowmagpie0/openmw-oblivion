#ifndef OPENMW_MWMECHANICS_OBLIVIONIDLE_H
#define OPENMW_MWMECHANICS_OBLIVIONIDLE_H

#include <optional>
#include <string>
#include <string_view>

#include <components/esm4/loadidle.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/vfs/pathutil.hpp>

namespace MWMechanics
{
    inline std::optional<std::string> oblivionIdleAnimationGroup(
        const ESM4::IdleAnimation& idle, std::string_view skeleton)
    {
        constexpr VFS::Path::ExtensionView kf("kf");
        const VFS::Path::Normalized idlePath
            = Misc::ResourceHelpers::correctMeshPath(idle.mModel.getNormalized());
        if (idlePath.extension() != kf)
            return std::nullopt;

        const VFS::Path::Normalized normalizedSkeleton(skeleton);
        const VFS::Path::Normalized skeletonDirectory(normalizedSkeleton.parent());
        const std::string_view directory = skeletonDirectory.value();
        const std::string_view path = idlePath.value();
        if (directory.empty() || path.size() <= directory.size() || !path.starts_with(directory)
            || path[directory.size()] != '/')
            return std::nullopt;
        return std::string(idlePath.stem());
    }

    inline std::optional<std::string> oblivionPickIdleAnimationGroup(
        const ESM4::IdleAnimation& idle, std::string_view skeleton)
    {
        if ((idle.mAnimationGroup & 0x7f) != 4)
            return std::nullopt;
        return oblivionIdleAnimationGroup(idle, skeleton);
    }
}

#endif
