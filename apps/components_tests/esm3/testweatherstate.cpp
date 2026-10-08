#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/weatherstate.hpp>

#include <gtest/gtest.h>

#include <functional>
#include <sstream>

namespace
{
    std::string weatherBytes(const std::function<void(ESM::ESMWriter&)>& write, bool checkFailureLeavesNoFields = false)
    {
        std::stringstream output;
        ESM::ESMWriter writer;
        writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
        writer.save(output);
        writer.startRecord(ESM::REC_WTHR);
        const auto before = output.str();
        try { write(writer); }
        catch (...)
        {
            if (checkFailureLeavesNoFields)
                EXPECT_EQ(output.str(), before);
            throw;
        }
        writer.endRecord(ESM::REC_WTHR);
        return output.str();
    }

    ESM::WeatherState readWeather(const std::string& bytes)
    {
        ESM::ESMReader reader;
        reader.open(std::make_unique<std::stringstream>(bytes), "weather-identity-fixture");
        reader.getRecName(); reader.getRecHeader();
        const std::map<int, int> removedContent{};
        reader.setContentFileMapping(&removedContent);
        ESM::WeatherState state{};
        state.load(reader, true);
        if (reader.hasMoreSubs())
            throw std::runtime_error("unexpected weather subrecords");
        return state;
    }

    ESM::WeatherState legacyWeather()
    {
        ESM::WeatherState state{};
        state.mCurrentWeather = 0;
        state.mNextWeather = state.mQueuedWeather = -1;
        return state;
    }
}

TEST(Esm3WeatherStateTest, NativeIdentityCatalogAndSelectionOrderRoundTripWithoutContentRemapping)
{
    auto state = legacyWeather();
    const auto region = ESM::RefId::stringRefId("climate");
    const std::vector<ESM::FormKey> keys{ESM::FormKey::content("Weather.esm", 0x123),
        ESM::FormKey::content("Other.esp", 0x456), ESM::FormKey::content("Weather.esm", 0x789)};
    state.mWeatherIdentities = keys;
    state.mCurrentWeather = 2; state.mNextWeather = 1; state.mQueuedWeather = 0;
    state.mWeatherOverride = true;
    state.mTimePassed = -1.25f; state.mWeatherUpdateTime = -3.f; state.mTransitionFactor = .375f;
    state.mRegions[region] = {1, {100, 25, 0, 255}, {2, 0, 1, -1}, 2};
    const auto bytes = weatherBytes([&](auto& writer) { state.save(writer); });
    const auto loaded = readWeather(bytes);
    ASSERT_TRUE(loaded.mWeatherIdentities);
    EXPECT_EQ(*loaded.mWeatherIdentities, keys);
    EXPECT_EQ(loaded.mCurrentWeather, 2); EXPECT_EQ(loaded.mNextWeather, 1); EXPECT_EQ(loaded.mQueuedWeather, 0);
    EXPECT_EQ(loaded.mWeatherOverride, true);
    EXPECT_EQ(loaded.mTimePassed, -1.25f); EXPECT_EQ(loaded.mWeatherUpdateTime, -3.f);
    EXPECT_EQ(loaded.mTransitionFactor, .375f);
    EXPECT_EQ(loaded.mRegions.at(region).mChances, state.mRegions.at(region).mChances);
    EXPECT_EQ(loaded.mRegions.at(region).mSelectionOrder, (std::vector<int32_t>{2, 0, 1, -1}));
    EXPECT_EQ(loaded.mRegions.at(region).mFallbackWeather, 2);
    EXPECT_EQ(weatherBytes([&](auto& writer) { loaded.save(writer); }), bytes);
}

TEST(Esm3WeatherStateTest, LegacyWeatherDoesNotWriteNativeIdentityMetadata)
{
    auto state = legacyWeather();
    const auto region = ESM::RefId::stringRefId("climate");
    state.mRegions[region] = {-1, {100, 255}};
    const auto bytes = weatherBytes([&](auto& writer) { state.save(writer); });
    EXPECT_EQ(bytes.find("WXVR"), std::string::npos);
    EXPECT_EQ(bytes.find("WXID"), std::string::npos);
    EXPECT_EQ(bytes.find("RGIX"), std::string::npos);
    EXPECT_EQ(bytes.find("RGDF"), std::string::npos);
    const auto loaded = readWeather(bytes);
    EXPECT_FALSE(loaded.mWeatherIdentities);
    EXPECT_TRUE(loaded.mRegions.at(region).mSelectionOrder.empty());
    EXPECT_EQ(loaded.mRegions.at(region).mFallbackWeather, 0);
    EXPECT_EQ(weatherBytes([&](auto& writer) { loaded.save(writer); }), bytes);
}

TEST(Esm3WeatherStateTest, InvalidIdentityVersionsKeysAndPartialOrdersRejectDuringDecode)
{
    const auto key = ESM::FormKey::content("weather.esm", 0x123).serialize();
    for (int fault = 0; fault != 9; ++fault)
    {
        SCOPED_TRACE(fault);
        const auto bytes = weatherBytes([&](auto& writer) {
            legacyWeather().save(writer);
            if (fault != 5 && fault != 7)
                writer.writeHNT("WXVR", uint32_t{fault == 0 ? 2u : 1u});
            if (fault != 0 && fault != 1 && fault != 7)
            {
                writer.writeHNString("WXID", fault == 2 ? "broken"
                    : fault == 4 ? ESM::FormKey::dynamic("weather", 1).serialize() : key);
                if (fault == 3) writer.writeHNString("WXID", key);
            }
            if (fault >= 6)
            {
                writer.writeHNRefId("RGNN", ESM::RefId::stringRefId("climate"));
                writer.writeHNT("RGNW", -1);
                if (fault == 7) writer.writeHNT("RGDF", 1);
                writer.writeHNT("RGNC", uint8_t{10});
                if (fault == 6) writer.writeHNT("RGIX", 0);
                writer.writeHNT("RGNC", uint8_t{90});
                if (fault == 8) writer.writeHNT("RGIX", 0);
            }
        });
        EXPECT_THROW(readWeather(bytes), std::runtime_error);
    }
}

TEST(Esm3WeatherStateTest, SavingInvalidSelectionMetadataRejectsBeforeWritingWeatherFields)
{
    for (int fault = 0; fault != 4; ++fault)
    {
        SCOPED_TRACE(fault);
        auto state = legacyWeather();
        state.mRegions[ESM::RefId::stringRefId("climate")] = {-1, {100}, {0}, 0};
        if (fault != 0)
            state.mWeatherIdentities = std::vector{ESM::FormKey::content("weather.esm", 0x123)};
        if (fault == 1) state.mWeatherIdentities->clear();
        if (fault == 2) state.mWeatherIdentities->push_back(state.mWeatherIdentities->front());
        if (fault == 3) state.mRegions.begin()->second.mSelectionOrder.push_back(0);
        EXPECT_THROW(weatherBytes([&](auto& writer) { state.save(writer); }, true), std::runtime_error);
    }
}
