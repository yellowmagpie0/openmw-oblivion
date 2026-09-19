#include <components/resource/oblivionui.hpp>
#include <components/testing/util.hpp>
#include <components/vfs/recursivedirectoryiterator.hpp>

#include <osg/Image>
#include <osgDB/Registry>

#include <gtest/gtest.h>

namespace
{
    struct Overlay : TestingOpenMW::VFSTestData
    {
        using VFSTestData::VFSTestData;
        void listResources(VFS::FileMap& out) override
        {
            for (const auto& [key, file] : mFiles)
                out.insert_or_assign(key, file);
        }
    };

    TEST(OblivionUi, EveryAuthoredTextureDecodesAndUnknownAssetsRemainMissing)
    {
        VFS::Manager vfs;
        auto archive = Resource::makeOblivionUiArchive(vfs);
        VFS::FileMap files;
        archive->listResources(files);
        ASSERT_EQ(files.size(), 109);
        auto* decoder = osgDB::Registry::instance()->getReaderWriterForExtension("dds");
        ASSERT_NE(decoder, nullptr);
        unsigned generated = 0, nativeIcons = 0;
        for (const auto& [path, file] : files)
        {
            if (path.value().starts_with("icons/"))
            {
                ++nativeIcons;
                // Native icon aliases must not conceal missing game archives.
                EXPECT_THROW(file->open(), std::runtime_error);
                continue;
            }
            ++generated;
            auto stream = file->open();
            auto result = decoder->readImage(*stream);
            ASSERT_TRUE(result.validImage()) << path.value() << ": " << result.message();
            const auto* image = result.getImage();
            EXPECT_GE(image->s(), 4);
            EXPECT_GE(image->t(), 4);
            EXPECT_EQ(image->getPixelFormat(), GL_RGBA);
        }
        EXPECT_EQ(generated, 106);
        EXPECT_EQ(nativeIcons, 3);
        EXPECT_FALSE(archive->contains(VFS::Path::NormalizedView("textures/missing-mod.dds")));
        EXPECT_FALSE(archive->contains(VFS::Path::NormalizedView("meshes/missing.nif")));
    }

    TEST(OblivionUi, CrosshairAndDamageOverlayPreserveClearView)
    {
        VFS::Manager vfs;
        vfs.addArchive(Resource::makeOblivionUiArchive(vfs));
        vfs.buildIndex();
        auto* decoder = osgDB::Registry::instance()->getReaderWriterForExtension("dds");
        ASSERT_NE(decoder, nullptr);
        auto input = vfs.get(VFS::Path::NormalizedView("textures/target.dds"));
        auto result = decoder->readImage(*input);
        ASSERT_TRUE(result.validImage());
        const auto* image = result.getImage();
        EXPECT_EQ(image->s(), 32);
        EXPECT_EQ(image->t(), 32);
        EXPECT_EQ(image->getColor(16, 16).a(), 0.f);
        EXPECT_EQ(image->getColor(0, 0).a(), 0.f);
        EXPECT_GT(image->getColor(16, 8).a(), 0.9f);
        input = vfs.get(VFS::Path::NormalizedView("textures/player_hit_01.dds"));
        result = decoder->readImage(*input);
        ASSERT_TRUE(result.validImage());
        image = result.getImage();
        EXPECT_EQ(image->getColor(128, 128).a(), 0.f);
        EXPECT_GT(image->getColor(0, 0).a(), 0.6f);
        EXPECT_GT(image->getColor(0, 0).r(), image->getColor(0, 0).g());
    }

    TEST(OblivionUi, GameAndModArtOverrideDefaultsAndNativeIconsUseActualBytes)
    {
        VFS::Manager vfs;
        TestingOpenMW::VFSTestFile override("mod texture");
        TestingOpenMW::VFSTestFile icon("native icon");
        vfs.addArchive(Resource::makeOblivionUiArchive(vfs));
        VFS::FileMap files;
        files.emplace(VFS::Path::Normalized("textures/target.dds"), &override);
        files.emplace(VFS::Path::Normalized("textures/menus/icons/weapons/handtohand.dds"), &icon);
        vfs.addArchive(std::make_unique<Overlay>(std::move(files)));
        vfs.buildIndex();
        const auto read = [&vfs](VFS::Path::NormalizedView path) {
            auto stream = vfs.get(path);
            return std::string(std::istreambuf_iterator<char>(*stream), std::istreambuf_iterator<char>());
        };
        EXPECT_EQ(read(VFS::Path::NormalizedView("textures/target.dds")), "mod texture");
        EXPECT_EQ(read(VFS::Path::NormalizedView("icons/k/stealth_handtohand.dds")), "native icon");
        EXPECT_TRUE(vfs.exists(VFS::Path::NormalizedView("textures/menu_bar_gray.dds")));
        EXPECT_THROW(read(VFS::Path::NormalizedView("textures/unknown.dds")), std::runtime_error);
    }
}
