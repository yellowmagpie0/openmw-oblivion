#ifndef OPENMW_MWSOUND_NATIVESOUNDDIRECTORY_H
#define OPENMW_MWSOUND_NATIVESOUNDDIRECTORY_H

#include <components/vfs/pathutil.hpp>

#include <vector>

namespace VFS { class Manager; }

namespace MWSound
{
    // Collect supported audio files from a TES4 SOUN directory, in stable order.
    std::vector<VFS::Path::Normalized> nativeSoundDirectoryFiles(
        const VFS::Manager& vfs, VFS::Path::NormalizedView directory);
}

#endif
