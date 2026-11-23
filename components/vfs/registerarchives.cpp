#include "registerarchives.hpp"

#include <filesystem>
#include <set>
#include <stdexcept>

#include <components/debug/debuglog.hpp>
#include <components/esm/gameprofile.hpp>
#include <components/misc/strings/algorithm.hpp>

#include <components/vfs/bsaarchive.hpp>
#include <components/vfs/filesystemarchive.hpp>
#include <components/vfs/manager.hpp>

namespace VFS
{
    std::vector<std::string> discoverDefaultArchives(ESM::GameProfile profile, const Files::Collections& collections,
        const std::vector<std::string>& contentFiles)
    {
        std::vector<std::string> archives;
        const auto defaults = ESM::getDefaultArchives(profile);
        if (defaults.empty() && profile != ESM::GameProfile::Oblivion)
            return archives;

        const auto& installed = collections.getCollection("bsa");
        std::set<std::string, Misc::StringUtils::CiComp> seen;
        const auto append = [&](std::string_view name) {
            if (seen.emplace(name).second)
                archives.emplace_back(name);
        };
        for (const auto name : defaults)
        {
            if (installed.doesExist(name))
                append(name);
            else
                Log(Debug::Warning) << "Default " << ESM::toString(profile) << " archive is unavailable: " << name;
        }

        if (profile == ESM::GameProfile::Oblivion)
        {
            for (const auto& plugin : contentFiles)
            {
                if (!Misc::StringUtils::ciEndsWith(plugin, ".esp") && !Misc::StringUtils::ciEndsWith(plugin, ".esm"))
                    continue;
                const std::string stem = plugin.substr(0, plugin.size() - 4);
                const std::string namesake = stem + ".bsa";
                if (installed.doesExist(namesake))
                    append(namesake);

                // MultiDirCollection iterates in case-insensitive filename order.
                const std::string prefix = stem + " - ";
                for (const auto& [name, path] : installed)
                    if (name.size() > prefix.size() + 4 && Misc::StringUtils::ciStartsWith(name, prefix)
                        && std::filesystem::is_regular_file(path))
                        append(name);
            }
        }
        return archives;
    }

    void registerArchives(VFS::Manager* vfs, const Files::Collections& collections,
        const std::vector<std::string>& archives, bool useLooseFiles, const ToUTF8::StatelessUtf8Encoder* encoder)
    {
        const Files::PathContainer& dataDirs = collections.getPaths();

        for (std::vector<std::string>::const_iterator archive = archives.begin(); archive != archives.end(); ++archive)
        {
            if (collections.doesExist(*archive))
            {
                // Last BSA has the highest priority
                const auto archivePath = collections.getPath(*archive);
                Log(Debug::Info) << "Adding BSA archive " << archivePath;
                vfs->addArchive(makeBsaArchive(archivePath, encoder));
            }
            else
            {
                throw std::runtime_error("Archive '" + *archive + "' not found");
            }
        }

        if (useLooseFiles)
        {
            std::set<std::filesystem::path> seen;
            for (const auto& dataDir : dataDirs)
            {
                if (seen.insert(dataDir).second)
                {
                    Log(Debug::Info) << "Adding data directory " << dataDir;
                    // Last data dir has the highest priority
                    vfs->addArchive(std::make_unique<FileSystemArchive>(dataDir));
                }
                else
                    Log(Debug::Info) << "Ignoring duplicate data directory " << dataDir;
            }
        }

        vfs->buildIndex();
    }

}
