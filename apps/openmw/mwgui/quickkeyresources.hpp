#ifndef OPENMW_MWGUI_QUICKKEYRESOURCES_H
#define OPENMW_MWGUI_QUICKKEYRESOURCES_H

#include <string>

#include <components/debug/debuglog.hpp>
#include <components/esm/gameprofile.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/vfs/manager.hpp>

namespace MWGui::QuickKeyResources
{
    inline VFS::Path::Normalized inventoryIcon(VFS::Path::NormalizedView icon, const VFS::Manager& vfs)
    {
        constexpr VFS::Path::NormalizedView defaultIcon("default icon.tga");
        if (icon.empty())
            icon = defaultIcon;
        auto result = Misc::ResourceHelpers::correctIconPath(icon, vfs);
        if (!vfs.exists(result))
        {
            Log(Debug::Error) << "Failed to open image: '" << result << "' not found, falling back to '"
                              << defaultIcon.value() << "'";
            result = Misc::ResourceHelpers::correctIconPath(defaultIcon, vfs);
        }
        return result;
    }

    inline std::string frame(std::string name, ESM::GameProfile profile, const VFS::Manager& vfs)
    {
        constexpr VFS::Path::NormalizedView fallback("textures/omw_menu_icon_active.dds");
        if (!name.empty() && profile == ESM::GameProfile::Oblivion
            && !vfs.exists(VFS::Path::Normalized(name)) && vfs.exists(fallback))
            return std::string(fallback.value());
        return name;
    }
}

#endif
