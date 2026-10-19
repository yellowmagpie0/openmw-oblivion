#include <gtest/gtest.h>

#include "../../openmw/mwsound/nativeaudioutils.hpp"
#include "../../openmw/mwsound/nativesounddirectory.hpp"

#include <components/testing/util.hpp>

namespace
{
    using namespace MWSound;

    TEST(NativeAudioTest, selectsEveryOblivionMusicState)
    {
        EXPECT_EQ(nativeMusicPlaylist(false, true, 0), "music/explore");
        EXPECT_EQ(nativeMusicPlaylist(false, false, 0), "music/dungeon");
        EXPECT_EQ(nativeMusicPlaylist(false, false, 1), "music/public");
        EXPECT_EQ(nativeMusicPlaylist(false, false, 2), "music/dungeon");
        EXPECT_EQ(nativeMusicPlaylist(true, true, 0), "music/battle");
        EXPECT_EQ(nativeMusicPlaylist(true, false, 1), "music/battle");
    }

    TEST(NativeAudioTest, convertsOblivionAttenuationAndDefaults)
    {
        const NativeSoundParams explicitRange = makeNativeSoundParams(2000, 5, 40, 5.f, 40.f, 20.f, 50.f);
        EXPECT_FLOAT_EQ(explicitRange.mVolume, 0.1f);
        EXPECT_FLOAT_EQ(explicitRange.mMinDistance, 100.f);
        EXPECT_FLOAT_EQ(explicitRange.mMaxDistance, 2000.f);

        const NativeSoundParams defaultRange = makeNativeSoundParams(0, 0, 0, 10.f, 60.f, 20.f, 50.f);
        EXPECT_FLOAT_EQ(defaultRange.mVolume, 1.f);
        EXPECT_FLOAT_EQ(defaultRange.mMinDistance, 200.f);
        EXPECT_FLOAT_EQ(defaultRange.mMaxDistance, 3000.f);

        const NativeSoundParams orderedRange = makeNativeSoundParams(0, 20, 1, 5.f, 40.f, 20.f, 50.f);
        EXPECT_FLOAT_EQ(orderedRange.mMinDistance, 400.f);
        EXPECT_FLOAT_EQ(orderedRange.mMaxDistance, 400.f);
    }

    TEST(NativeAudioTest, decodesRegionWeatherMasksAndFixedPointChance)
    {
        EXPECT_EQ(nativeRegionWeatherMask(1), 1u);
        EXPECT_EQ(nativeRegionWeatherMask(2), 2u);
        EXPECT_EQ(nativeRegionWeatherMask(4), 4u);
        EXPECT_EQ(nativeRegionWeatherMask(8), 8u);
        EXPECT_EQ(nativeRegionWeatherMask(10), 8u);
        EXPECT_EQ(nativeRegionSoundChance(1500000), 15u);
        EXPECT_EQ(nativeRegionSoundChance(10000000), 100u);
        EXPECT_EQ(nativeRegionSoundChance(50000000), 100u);
        EXPECT_TRUE(nativeRegionSoundMatches(11, 1500000, 1, 14));
        EXPECT_FALSE(nativeRegionSoundMatches(11, 1500000, 4, 0));
        EXPECT_FALSE(nativeRegionSoundMatches(11, 1500000, 1, 15));
        EXPECT_TRUE(nativeRegionSoundMatches(15, 10000000, 8, 99));
    }
}

TEST(NativeAudioTest, NativeDirectoryDoesNotSelectSiblingPrefixes)
{
    using VFS::Path::Normalized;
    const auto vfs = TestingOpenMW::createTestVFS({
        {Normalized("sound/fx/bladeflesh/c.wav"), nullptr},
        {Normalized("sound/fx/bladeflesh/a.mp3"), nullptr},
        {Normalized("sound/fx/bladeflesh/nested/b.ogg"), nullptr},
        {Normalized("sound/fx/bladeflesh/d.flac"), nullptr},
        {Normalized("sound/fx/bladeflesh/readme.txt"), nullptr},
        {Normalized("sound/fx/bladefleshenchanted/magic.wav"), nullptr},
        {Normalized("sound/fx/bladeflesh2/other.wav"), nullptr},
        {Normalized("sound/fx/bladeflesh.wav"), nullptr},
        {Normalized("sound/fx/blade/swing.wav"), nullptr},
    });
    const std::vector<Normalized> expected{
        Normalized("sound/fx/bladeflesh/a.mp3"), Normalized("sound/fx/bladeflesh/c.wav"),
        Normalized("sound/fx/bladeflesh/d.flac"), Normalized("sound/fx/bladeflesh/nested/b.ogg")};
    EXPECT_EQ(MWSound::nativeSoundDirectoryFiles(*vfs, VFS::Path::NormalizedView("sound/fx/bladeflesh")), expected);
    EXPECT_EQ(MWSound::nativeSoundDirectoryFiles(*vfs, VFS::Path::NormalizedView("sound/fx/bladeflesh/")), expected);
}

TEST(NativeAudioTest, NativeDirectoryMissingDoesNotBorrowSiblingAudio)
{
    const auto vfs = TestingOpenMW::createTestVFS({
        {VFS::Path::Normalized("sound/fx/bladefleshenchanted/magic.wav"), nullptr}});
    EXPECT_TRUE(MWSound::nativeSoundDirectoryFiles(*vfs,
        VFS::Path::NormalizedView("sound/fx/bladeflesh")).empty());
}
