#ifndef OPENMW_COMPONENTS_RESOURCE_OBLIVIONUI_H
#define OPENMW_COMPONENTS_RESOURCE_OBLIVIONUI_H

#include <memory>

namespace VFS
{
    class Archive;
    class Manager;
}

namespace Resource
{
    // Engine-authored shared UI for the Oblivion profile. Register before game
    // archives so explicit game/mod resources retain their normal precedence.
    // This supplies only named UI assets, never arbitrary missing resources.
    std::unique_ptr<VFS::Archive> makeOblivionUiArchive(const VFS::Manager& vfs);
}

#endif
