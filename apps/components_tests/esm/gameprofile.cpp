#include <components/esm/gameprofile.hpp>

#include <gtest/gtest.h>

namespace
{
    TEST(GameProfileTest, parsesNamesAndAliases)
    {
        EXPECT_EQ(ESM::parseGameProfile("AUTO"), ESM::GameProfile::Auto);
        EXPECT_EQ(ESM::parseGameProfile("tes3"), ESM::GameProfile::Morrowind);
        EXPECT_EQ(ESM::parseGameProfile("Oblivion"), ESM::GameProfile::Oblivion);
        EXPECT_THROW(ESM::parseGameProfile("fallout"), std::invalid_argument);
    }

    TEST(GameProfileTest, automaticallySelectsFromContentFormat)
    {
        ESM::GameProfileSelector selector;
        EXPECT_EQ(selector.observe(ESM::Format::Tes4, "Oblivion.esm"), ESM::GameProfile::Oblivion);
        EXPECT_EQ(selector.observe(ESM::Format::Tes4, "Knights.esp"), ESM::GameProfile::Oblivion);
    }

    TEST(GameProfileTest, rejectsExplicitMismatch)
    {
        ESM::GameProfileSelector selector(ESM::GameProfile::Morrowind);
        try
        {
            selector.observe(ESM::Format::Tes4, "Oblivion.esm");
            FAIL() << "Expected explicit game-profile mismatch";
        }
        catch (const std::runtime_error& error)
        {
            const std::string message = error.what();
            EXPECT_NE(message.find("morrowind"), std::string::npos);
            EXPECT_NE(message.find("Oblivion.esm"), std::string::npos);
            EXPECT_NE(message.find("oblivion"), std::string::npos);
        }
    }

    TEST(GameProfileTest, rejectsMixedFormats)
    {
        ESM::GameProfileSelector selector;
        selector.observe(ESM::Format::Tes3, "Morrowind.esm");
        EXPECT_THROW(selector.observe(ESM::Format::Tes4, "Oblivion.esm"), std::runtime_error);
    }

    TEST(GameProfileTest, suppliesOnlyOblivionNativeArchiveDefaults)
    {
        EXPECT_TRUE(ESM::getDefaultArchives(ESM::GameProfile::Auto).empty());
        EXPECT_TRUE(ESM::getDefaultArchives(ESM::GameProfile::Morrowind).empty());
        const auto archives = ESM::getDefaultArchives(ESM::GameProfile::Oblivion);
        ASSERT_EQ(archives.size(), 6);
        EXPECT_EQ(archives.front(), "Oblivion - Meshes.bsa");
        EXPECT_EQ(archives.back(), "Oblivion - Voices2.bsa");
    }
}
