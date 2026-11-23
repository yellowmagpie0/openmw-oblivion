#ifndef OPENMW_COMPONENTS_VFS_REGISTER_ARCHIVES_H
#define OPENMW_COMPONENTS_VFS_REGISTER_ARCHIVES_H

#include <components/files/collections.hpp>

namespace ESM
{
    enum class GameProfile;
}

namespace ToUTF8
{
    class StatelessUtf8Encoder;
}

namespace VFS
{
    class Manager;

    /// Resolve profile defaults and installed plugin BSAs in increasing mount priority.
    std::vector<std::string> discoverDefaultArchives(ESM::GameProfile profile, const Files::Collections& collections,
        const std::vector<std::string>& contentFiles);

    /// @brief Register BSA and file system archives based on the given OpenMW configuration.
    void registerArchives(VFS::Manager* vfs, const Files::Collections& collections,
        const std::vector<std::string>& archives, bool useLooseFiles, const ToUTF8::StatelessUtf8Encoder* encoder);
}

#endif
