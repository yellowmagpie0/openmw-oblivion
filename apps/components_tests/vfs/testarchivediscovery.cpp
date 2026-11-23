#include <components/esm/gameprofile.hpp>
#include <components/testing/util.hpp>
#include <components/vfs/registerarchives.hpp>

#include <fstream>

namespace
{
    TEST(VFSArchiveDiscovery, LoadsNamesakesThenSplitArchivesInPluginOrder)
    {
        const auto dir = TestingOpenMW::currentTestDirPath();
        for (const auto name : { "Grove.bsa", "Grove - Textures.bsa", "GROVE - meshes.BSA", "Grove - .bsa",
                 "Groves - Extra.bsa", "Inactive.bsa", "Inactive - Trees.bsa", "Birch - Custom.bsa",
                 "Grove - Loose.esp", "Ignored.bsa" })
            ASSERT_TRUE(std::ofstream(dir / name));

        const Files::Collections files({ dir });
        const std::vector<std::string> plugins{ "Grove.ESP", "Birch.eSm", "grove.esm", "Ignored.omwaddon" };
        const std::vector<std::string> expected{ "Grove.bsa", "GROVE - meshes.BSA", "Grove - Textures.bsa",
            "Birch - Custom.bsa" };
        EXPECT_EQ(VFS::discoverDefaultArchives(ESM::GameProfile::Oblivion, files, plugins), expected);
        EXPECT_TRUE(VFS::discoverDefaultArchives(ESM::GameProfile::Morrowind, files, plugins).empty());
    }

    TEST(VFSArchiveDiscovery, KeepsProfileDefaultOrderWhenMasterAlsoMatches)
    {
        const auto dir = TestingOpenMW::currentTestDirPath();
        for (const auto name : { "Oblivion - Misc.bsa", "oblivion - textures - compressed.BSA", "Oblivion.bsa" })
            ASSERT_TRUE(std::ofstream(dir / name));

        const std::vector<std::string> expected{ "Oblivion - Textures - Compressed.bsa", "Oblivion - Misc.bsa",
            "Oblivion.bsa" };
        EXPECT_EQ(VFS::discoverDefaultArchives(ESM::GameProfile::Oblivion, Files::Collections({ dir }), { "Oblivion.esm" }),
            expected);
    }
}
