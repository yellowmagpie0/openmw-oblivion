#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/savedgame.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <sstream>

namespace
{
    ESM::SavedGame makeProfile(ESM::GameProfile gameProfile)
    {
        ESM::SavedGame profile;
        profile.mContentFiles = { gameProfile == ESM::GameProfile::Oblivion ? "Oblivion.esm" : "Morrowind.esm" };
        profile.mPlayerName = "Prisoner";
        profile.mPlayerLevel = 1;
        profile.mPlayerClassName = "Adventurer";
        profile.mPlayerCellName = "Test Cell";
        profile.mInGameTime = { 12.f, 1, 0, 1 };
        profile.mDescription = "profile test";
        profile.mGameProfile = gameProfile;
        profile.mRuntimeStateVersion = gameProfile == ESM::GameProfile::Oblivion ? 1 : 0;
        return profile;
    }

    std::pair<ESM::SavedGame, std::string> roundTrip(const ESM::SavedGame& expected)
    {
        auto output = std::make_unique<std::stringstream>();
        ESM::ESMWriter writer;
        writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
        writer.save(*output);
        writer.startRecord(ESM::REC_SAVE);
        expected.save(writer);
        writer.endRecord(ESM::REC_SAVE);
        writer.close();
        const std::string bytes = output->str();

        std::unique_ptr<std::istream> input = std::move(output);
        ESM::ESMReader reader;
        reader.open(std::move(input), "saved-game-profile-stream");
        EXPECT_EQ(reader.getRecName(), ESM::REC_SAVE);
        reader.getRecHeader();
        ESM::SavedGame actual;
        actual.load(reader);
        return { std::move(actual), bytes };
    }

    TEST(SavedGameProfile, historicalUntaggedSchemaDefaultsToMorrowind)
    {
        const auto [actual, bytes] = roundTrip(makeProfile(ESM::GameProfile::Morrowind));
        EXPECT_EQ(actual.mGameProfile, ESM::GameProfile::Morrowind);
        EXPECT_EQ(actual.mRuntimeStateVersion, 0u);
        EXPECT_EQ(bytes.find("GPRO"), std::string::npos);
        EXPECT_EQ(bytes.find("T4VR"), std::string::npos);
    }

    TEST(SavedGameProfile, oblivionProfileAndRuntimeSchemaRoundTrip)
    {
        const auto [actual, bytes] = roundTrip(makeProfile(ESM::GameProfile::Oblivion));
        EXPECT_EQ(actual.mGameProfile, ESM::GameProfile::Oblivion);
        EXPECT_EQ(actual.mRuntimeStateVersion, 1u);
        EXPECT_NE(bytes.find("GPRO"), std::string::npos);
        EXPECT_NE(bytes.find("T4VR"), std::string::npos);
    }

    TEST(SavedGameProfile, refusesAmbiguousOrIncompleteProfileTags)
    {
        ESM::SavedGame automatic = makeProfile(ESM::GameProfile::Auto);
        EXPECT_THROW(roundTrip(automatic), std::runtime_error);
        ESM::SavedGame oblivion = makeProfile(ESM::GameProfile::Oblivion);
        oblivion.mRuntimeStateVersion = 0;
        EXPECT_THROW(roundTrip(oblivion), std::runtime_error);
    }
}
