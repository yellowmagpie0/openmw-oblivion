#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <components/esm4/loadclmt.hpp>
#include <components/esm4/loadwthr.hpp>

#include "apps/openmw/mwworld/timestamp.hpp"
#include "apps/openmw/mwworld/weather.hpp"

namespace MWWorld
{
    namespace
    {
        TEST(MWWorldWeatherTest, moonPhasesHaveMwscriptCompatibleValues)
        {
            using Phase = MWRender::MoonState::Phase;

            EXPECT_EQ(MWRender::MoonState::phaseToInt(Phase::New), 0);
            EXPECT_EQ(MWRender::MoonState::phaseToInt(Phase::WaxingCrescent), 1);
            EXPECT_EQ(MWRender::MoonState::phaseToInt(Phase::WaningCrescent), 1);
            EXPECT_EQ(MWRender::MoonState::phaseToInt(Phase::FirstQuarter), 2);
            EXPECT_EQ(MWRender::MoonState::phaseToInt(Phase::ThirdQuarter), 2);
            EXPECT_EQ(MWRender::MoonState::phaseToInt(Phase::WaxingGibbous), 3);
            EXPECT_EQ(MWRender::MoonState::phaseToInt(Phase::WaningGibbous), 3);
            EXPECT_EQ(MWRender::MoonState::phaseToInt(Phase::Full), 4);
        }

        TEST(MWWorldWeatherTest, oblivionClimateTimingAndMoonFlagsAreDecodedExactly)
        {
            ESM4::Climate climate;
            climate.mTiming.mSunriseBegin = 36;
            climate.mTiming.mSunsetEnd = 114;
            climate.mTiming.mMoonsPhase = ESM4::Climate::Moon_Masser | ESM4::Climate::Moon_Secunda | 5;

            EXPECT_FLOAT_EQ(ESM4::Climate::decodeTime(climate.mTiming.mSunriseBegin), 6.f);
            EXPECT_FLOAT_EQ(ESM4::Climate::decodeTime(climate.mTiming.mSunsetEnd), 19.f);
            EXPECT_TRUE(climate.hasMasser());
            EXPECT_TRUE(climate.hasSecunda());
            EXPECT_EQ(climate.phaseLength(), 5u);
        }

        TEST(MWWorldWeatherTest, oblivionWeatherMapsExactFogPrecipitationAndCloudData)
        {
            ESM4::Weather native;
            native.mId = ESM::FormId::fromUint32(0x1234);
            native.mEditorId = "TestRain";
            native.mLowerCloudTexture = "Sky\\CloudsLower.dds";
            native.mFog = { 100.f, 2000.f, 300.f, 1200.f };
            native.mData.mWindSpeed = 128;
            native.mData.mTransitionDelta = 51;
            native.mData.mSunGlare = 153;
            native.mData.mClassification = ESM4::Weather::Classification_Rainy;

            const Weather weather(native, 7);
            EXPECT_EQ(weather.mId, ESM::RefId(native.mId));
            EXPECT_EQ(weather.mScriptId, 7);
            EXPECT_EQ(weather.mName, "TestRain");
            EXPECT_EQ(weather.mCloudTexture, "textures/Sky\\CloudsLower.dds");
            EXPECT_TRUE(weather.mUseExactFog);
            EXPECT_FLOAT_EQ(weather.mFogNear.getDayValue(), 100.f);
            EXPECT_FLOAT_EQ(weather.mFogFar.getDayValue(), 2000.f);
            EXPECT_FLOAT_EQ(weather.mFogNear.getNightValue(), 300.f);
            EXPECT_FLOAT_EQ(weather.mFogFar.getNightValue(), 1200.f);
            EXPECT_FLOAT_EQ(weather.transitionDelta(), 51.f / 255.f);
            EXPECT_FLOAT_EQ(weather.mGlareView, 153.f / 255.f);
            EXPECT_FALSE(weather.mRainEffect.empty());
            EXPECT_TRUE(weather.mParticleEffect.empty());

            native.mData.mClassification = ESM4::Weather::Classification_Snow;
            const Weather snow(native, 8);
            EXPECT_TRUE(snow.mRainEffect.empty());
            EXPECT_FALSE(snow.mParticleEffect.empty());
        }

        TEST(MWWorldWeatherTest, nativeWeatherRestoreStagesFreshRegionsAndChecksConsumedIndexDomains)
        {
            const auto known = ESM::RefId::stringRefId("known-climate");
            const auto added = ESM::RefId::stringRefId("added-climate");
            const auto removed = ESM::RefId::stringRefId("removed-climate");
            for (std::size_t count : {1u, 3u, 10u, 17u})
            for (int field = 0; field != 4; ++field)
            for (int index : {-2, -1, 0, int(count - 1), int(count), std::numeric_limits<int>::max()})
            {
                SCOPED_TRACE(count);
                SCOPED_TRACE(field);
                SCOPED_TRACE(index);
                ESM::WeatherState state{};
                state.mNextWeather = state.mQueuedWeather = -1;
                state.mRegions[known] = {-1, {100}};
                // Removed data is ignored even when its values cannot be consumed.
                state.mRegions[removed] = {std::numeric_limits<int>::max(), {0, 0, 0, 100}};
                if (field == 0) state.mCurrentWeather = index;
                if (field == 1) state.mNextWeather = index;
                if (field == 2) state.mQueuedWeather = index;
                if (field == 3) state.mRegions[known].mWeather = index;
                std::map<ESM::RefId, RegionWeather> defaults;
                defaults.emplace(known, RegionWeather(std::vector<uint8_t>{0, 100}));
                defaults.emplace(added, RegionWeather(std::vector<uint8_t>{100}));
                const bool accepted = (index >= 0 && std::size_t(index) < count) || (field != 0 && index == -1);
                if (!accepted)
                    EXPECT_THROW(prepareWeatherRestore(state, count, defaults), std::runtime_error);
                else
                {
                    auto prepared = prepareWeatherRestore(state, count, defaults);
                    state.mRegions.clear(); // The plan owns its saved overlay.
                    ASSERT_EQ(prepared.mRegions.size(), 2u);
                    EXPECT_EQ(ESM::RegionWeatherState(prepared.mRegions.at(known)).mChances,
                        std::vector<uint8_t>({100}));
                    EXPECT_EQ(ESM::RegionWeatherState(prepared.mRegions.at(added)).mWeather, -1);
                    EXPECT_EQ(ESM::RegionWeatherState(prepared.mRegions.at(added)).mChances,
                        std::vector<uint8_t>({100}));
                }
                // Neither successful nor rejected preparation changes caller defaults.
                EXPECT_EQ(ESM::RegionWeatherState(defaults.at(known)).mChances,
                    std::vector<uint8_t>({0, 100}));
                EXPECT_EQ(ESM::RegionWeatherState(defaults.at(known)).mWeather, -1);
            }
        }

        TEST(MWWorldWeatherTest, nativeWeatherRestorePreservesUnreachableProbabilityTailsAndFiniteCountdowns)
        {
            const auto region = ESM::RefId::stringRefId("climate");
            std::map<ESM::RefId, RegionWeather> defaults;
            defaults.emplace(region, RegionWeather(std::vector<uint8_t>{100}));
            ESM::WeatherState state{};
            state.mNextWeather = state.mQueuedWeather = -1;
            state.mTimePassed = -3.f;
            state.mWeatherUpdateTime = -7.f;
            state.mTransitionFactor = 1.25f; // Domain enforcement follows consumption, not an invented clamp.
            for (const auto& chances : std::initializer_list<std::vector<uint8_t>>{{}, {0}, {99, 0, 0}, {100, 255}, {255, 1}})
            {
                state.mRegions[region] = {-1, chances};
                auto prepared = prepareWeatherRestore(state, 1, defaults);
                EXPECT_EQ(ESM::RegionWeatherState(prepared.mRegions.at(region)).mChances, chances);
                EXPECT_EQ(prepared.mState.mTimePassed, -3.f);
                EXPECT_EQ(prepared.mState.mWeatherUpdateTime, -7.f);
                EXPECT_EQ(prepared.mState.mTransitionFactor, 1.25f);
            }
            for (const auto& chances : std::initializer_list<std::vector<uint8_t>>{{0, 1}, {99, 1}, {0, 0, 100}})
            {
                state.mRegions[region] = {-1, chances};
                EXPECT_THROW(prepareWeatherRestore(state, 1, defaults), std::runtime_error);
            }
            state.mRegions.clear();
            EXPECT_THROW(prepareWeatherRestore(state, 0, defaults), std::runtime_error);
            for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()})
            for (int field = 0; field != 3; ++field)
            {
                auto corrupt = state;
                if (field == 0) corrupt.mTimePassed = invalid;
                if (field == 1) corrupt.mWeatherUpdateTime = invalid;
                if (field == 2) corrupt.mTransitionFactor = invalid;
                EXPECT_THROW(prepareWeatherRestore(corrupt, 1, defaults), std::runtime_error);
            }
        }

        TEST(MWWorldWeatherTest, nativeIdentityCatalogSkipsUnusedMissingWeatherAndRejectsConsumedDependencies)
        {
            const auto region = ESM::RefId::stringRefId("catalog-climate");
            const std::vector<ESM::FormKey> old{ESM::FormKey::content("weather.esm", 1),
                ESM::FormKey::content("removed.esp", 2), ESM::FormKey::content("weather.esm", 3),
                ESM::FormKey::content("removed.esp", 4)};
            const std::vector<ESM::FormKey> loaded{old[2], old[0]};
            std::map<ESM::RefId, RegionWeather> defaults;
            defaults.emplace(region, RegionWeather(std::vector<uint8_t>{100}));
            ESM::WeatherState state{};
            state.mWeatherIdentities = old;
            state.mCurrentWeather = 0; state.mNextWeather = 2; state.mQueuedWeather = -1;
            state.mRegions[region] = {-1, {10, 0, 25, 0}};
            const auto prepared = prepareWeatherRestore(state, loaded.size(), defaults, loaded);
            EXPECT_EQ(prepared.mState.mCurrentWeather, 1); EXPECT_EQ(prepared.mState.mNextWeather, 0);
            EXPECT_EQ(prepared.mState.mRegions.at(region).mSelectionOrder,
                (std::vector<int32_t>{1, -1, 0, -1}));
            EXPECT_EQ(prepared.mState.mRegions.at(region).mFallbackWeather, 1);
            EXPECT_TRUE(state.mRegions.at(region).mSelectionOrder.empty()); // Detached input untouched.
            for (int fault = 0; fault != 11; ++fault)
            {
                SCOPED_TRACE(fault);
                auto corrupt = state;
                if (fault == 0) corrupt.mCurrentWeather = 1;
                if (fault == 1) corrupt.mNextWeather = 1;
                if (fault == 2) corrupt.mQueuedWeather = 3;
                if (fault == 3) corrupt.mRegions[region].mWeather = 1;
                if (fault == 4) corrupt.mRegions[region].mChances[1] = 1;
                if (fault == 5) corrupt.mRegions[region].mFallbackWeather = 1;
                if (fault == 6) corrupt.mCurrentWeather = 4;
                if (fault == 7) corrupt.mRegions[region].mSelectionOrder = {0};
                if (fault == 8) corrupt.mRegions[region].mSelectionOrder = {0, -2, 2, 3};
                if (fault == 9) corrupt.mRegions[region].mFallbackWeather = 4;
                if (fault == 10) corrupt.mWeatherIdentities->push_back(old[0]);
                EXPECT_THROW(prepareWeatherRestore(corrupt, loaded.size(), defaults, loaded), std::runtime_error);
            }
            auto removed = state;
            removed.mRegions[region].mWeather = 12345;
            EXPECT_NO_THROW(prepareWeatherRestore(removed, loaded.size(), {}, loaded));
            EXPECT_THROW(prepareWeatherRestore(state, 3, defaults, loaded), std::runtime_error);
            const std::vector<ESM::FormKey> duplicate{old[0], old[0]};
            EXPECT_THROW(prepareWeatherRestore(state, 2, defaults, duplicate), std::runtime_error);
            auto legacy = state;
            legacy.mWeatherIdentities.reset(); legacy.mNextWeather = -1;
            legacy.mRegions[region].mChances = {100, 255};
            EXPECT_EQ(prepareWeatherRestore(legacy, 2, defaults, loaded).mState.mCurrentWeather, 0);
        }

        TEST(MWWorldWeatherTest, nativeCatalogPreservesUnreachableLegacyTailsThroughRepeatedRestoration)
        {
            const auto region = ESM::RefId::stringRefId("climate");
            const std::vector<ESM::FormKey> keys{ESM::FormKey::content("weather.esm", 1)};
            ESM::WeatherState state{};
            state.mWeatherIdentities = keys;
            state.mNextWeather = state.mQueuedWeather = -1;
            state.mRegions[region] = {-1, {100, 255}};
            std::map<ESM::RefId, RegionWeather> defaults;
            defaults.emplace(region, RegionWeather(std::vector<uint8_t>{100}));
            const auto first = prepareWeatherRestore(state, 1, defaults, keys);
            const auto second = prepareWeatherRestore(first.mState, 1, defaults, keys);
            EXPECT_EQ(second.mState.mRegions.at(region).mChances, (std::vector<uint8_t>{100, 255}));
            EXPECT_EQ(second.mState.mRegions.at(region).mSelectionOrder, (std::vector<int32_t>{0, -1}));
        }

        // MASSER PHASES

        TEST(MWWorldWeatherTest, masserPhasesFullToWaningGibbousAtCorrectTimes)
        {
            float dailyIncrement = 1.0f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 35.0f;

            // Days 2 and 26, 11:57
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 2 + 11.0f + 56.0f / 60.0f);
            timeStampAfter += (24.0f * 2 + 11.0f + 58.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 26 + 11.0f + 56.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 26 + 11.0f + 58.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<MWRender::MoonState::Phase>(0));
            EXPECT_EQ(afterState.mPhase, static_cast<MWRender::MoonState::Phase>(1));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(0));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(1));
        }

        TEST(MWWorldWeatherTest, masserPhasesWaningGibbousToThirdQuarterAtCorrectTimes)
        {
            float dailyIncrement = 1.0f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 35.0f;

            // Days 5 and 29, 0:00
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 4 + 23.0f + 59.0f / 60.0f);
            timeStampAfter += (24.0f * 5 + 0.0f + 1.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 28 + 23.0f + 59.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 29 + 0.0f + 1.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<MWRender::MoonState::Phase>(1));
            EXPECT_EQ(afterState.mPhase, static_cast<MWRender::MoonState::Phase>(2));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(1));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(2));
        }

        TEST(MWWorldWeatherTest, masserPhasesThirdQuarterToWaningCrescentAtCorrectTimes)
        {
            float dailyIncrement = 1.0f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 35.0f;

            // Days 8 and 32, 0:00
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 7 + 23.0f + 59.0f / 60.0f);
            timeStampAfter += (24.0f * 8 + 0.0f + 1.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 31 + 23.0f + 59.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 32 + 0.0f + 1.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<MWRender::MoonState::Phase>(2));
            EXPECT_EQ(afterState.mPhase, static_cast<MWRender::MoonState::Phase>(3));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(2));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(3));
        }

        TEST(MWWorldWeatherTest, masserPhasesWaningCrescentToNewAtCorrectTimes)
        {
            float dailyIncrement = 1.0f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 35.0f;

            // Days 11 and 35, 0:00
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 10 + 23.0f + 59.0f / 60.0f);
            timeStampAfter += (24.0f * 11 + 0.0f + 1.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 34 + 23.0f + 59.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 35 + 0.0f + 1.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<MWRender::MoonState::Phase>(3));
            EXPECT_EQ(afterState.mPhase, static_cast<MWRender::MoonState::Phase>(4));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(3));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(4));
        }

        TEST(MWWorldWeatherTest, masserPhasesNewToWaxingCrescentAtCorrectTimes)
        {
            float dailyIncrement = 1.0f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 35.0f;

            // Days 14 and 38, 0:00
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 13 + 23.0f + 59.0f / 60.0f);
            timeStampAfter += (24.0f * 14 + 0.0f + 1.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 37 + 23.0f + 59.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 38 + 0.0f + 1.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<MWRender::MoonState::Phase>(4));
            EXPECT_EQ(afterState.mPhase, static_cast<MWRender::MoonState::Phase>(5));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(4));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(5));
        }

        TEST(MWWorldWeatherTest, masserPhasesWaxingCrescentToFirstQuarterAtCorrectTimes)
        {
            float dailyIncrement = 1.0f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 35.0f;

            // Days 17 and 41, 2:57
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 17 + 2.0f + 56.0f / 60.0f);
            timeStampAfter += (24.0f * 17 + 2.0f + 58.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 41 + 2.0f + 56.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 41 + 2.0f + 58.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<MWRender::MoonState::Phase>(5));
            EXPECT_EQ(afterState.mPhase, static_cast<MWRender::MoonState::Phase>(6));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(5));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(6));
        }

        TEST(MWWorldWeatherTest, masserPhasesFirstQuarterToWaxingGibbousAtCorrectTimes)
        {
            float dailyIncrement = 1.0f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 35.0f;

            // Days 20 and 44, 5:57
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 20 + 5.0f + 56.0f / 60.0f);
            timeStampAfter += (24.0f * 20 + 5.0f + 58.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 44 + 5.0f + 56.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 44 + 5.0f + 58.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<MWRender::MoonState::Phase>(6));
            EXPECT_EQ(afterState.mPhase, static_cast<MWRender::MoonState::Phase>(7));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(6));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(7));
        }

        TEST(MWWorldWeatherTest, masserPhasesWaxingGibbousToFullAtCorrectTimes)
        {
            float dailyIncrement = 1.0f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 35.0f;

            // Days 23 and 47, 8:57
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 23 + 8.0f + 56.0f / 60.0f);
            timeStampAfter += (24.0f * 23 + 8.0f + 58.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 47 + 8.0f + 56.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 47 + 8.0f + 58.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<MWRender::MoonState::Phase>(7));
            EXPECT_EQ(afterState.mPhase, static_cast<MWRender::MoonState::Phase>(0));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(7));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(0));
        }

        // SECUNDA PHASES

        TEST(MWWorldWeatherTest, secundaPhasesFullToWaningGibbousAtCorrectTimes)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 50.0f;

            // Days 2 and 26, 14:19
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 2 + 14.0f + 18.0f / 60.0f);
            timeStampAfter += (24.0f * 2 + 14.0f + 20.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 26 + 14.0f + 18.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 26 + 14.0f + 20.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<MWRender::MoonState::Phase>(0));
            EXPECT_EQ(afterState.mPhase, static_cast<MWRender::MoonState::Phase>(1));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(0));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(1));
        }

        TEST(MWWorldWeatherTest, secundaPhasesWaningGibbousToThirdQuarterAtCorrectTimes)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 50.0f;

            // Days 5 and 29, 0:00
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 4 + 23.0f + 59.0f / 60.0f);
            timeStampAfter += (24.0f * 5 + 0.0f + 1.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 28 + 23.0f + 59.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 29 + 0.0f + 1.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<MWRender::MoonState::Phase>(1));
            EXPECT_EQ(afterState.mPhase, static_cast<MWRender::MoonState::Phase>(2));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(1));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(2));
        }

        TEST(MWWorldWeatherTest, secundaPhasesThirdQuarterToWaningCrescentAtCorrectTimes)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 50.0f;

            // Days 8 and 32, 0:00
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 7 + 23.0f + 59.0f / 60.0f);
            timeStampAfter += (24.0f * 8 + 0.0f + 1.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 31 + 23.0f + 59.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 32 + 0.0f + 1.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<MWRender::MoonState::Phase>(2));
            EXPECT_EQ(afterState.mPhase, static_cast<MWRender::MoonState::Phase>(3));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(2));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(3));
        }

        TEST(MWWorldWeatherTest, secundaPhasesWaningCrescentToNewAtCorrectTimes)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 50.0f;

            // Days 11 and 35, 0:00
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 10 + 23.0f + 59.0f / 60.0f);
            timeStampAfter += (24.0f * 11 + 0.0f + 1.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 34 + 23.0f + 59.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 35 + 0.0f + 1.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<MWRender::MoonState::Phase>(3));
            EXPECT_EQ(afterState.mPhase, static_cast<MWRender::MoonState::Phase>(4));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(3));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(4));
        }

        TEST(MWWorldWeatherTest, secundaPhasesNewToWaxingCrescentAtCorrectTimes)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 50.0f;

            // Days 14 and 38, 0:00
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 13 + 23.0f + 59.0f / 60.0f);
            timeStampAfter += (24.0f * 14 + 0.0f + 1.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 37 + 23.0f + 59.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 38 + 0.0f + 1.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<MWRender::MoonState::Phase>(4));
            EXPECT_EQ(afterState.mPhase, static_cast<MWRender::MoonState::Phase>(5));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(4));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(5));
        }

        TEST(MWWorldWeatherTest, secundaPhasesWaxingCrescentToFirstQuarterAtCorrectTimes)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 50.0f;

            // Days 17 and 41, 3:31
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 17 + 3.0f + 30.0f / 60.0f);
            timeStampAfter += (24.0f * 17 + 3.0f + 32.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 41 + 3.0f + 30.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 41 + 3.0f + 32.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<MWRender::MoonState::Phase>(5));
            EXPECT_EQ(afterState.mPhase, static_cast<MWRender::MoonState::Phase>(6));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(5));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(6));
        }

        TEST(MWWorldWeatherTest, secundaPhasesFirstQuarterToWaxingGibbousAtCorrectTimes)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 50.0f;

            // Days 20 and 44, 7:07
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 20 + 7.0f + 6.0f / 60.0f);
            timeStampAfter += (24.0f * 20 + 7.0f + 8.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 44 + 7.0f + 6.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 44 + 7.0f + 8.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<MWRender::MoonState::Phase>(6));
            EXPECT_EQ(afterState.mPhase, static_cast<MWRender::MoonState::Phase>(7));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(6));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(7));
        }

        TEST(MWWorldWeatherTest, secundaPhasesWaxingGibbousToFullAtCorrectTimes)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 50.0f;

            // Days 23 and 47, 10:43
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 23 + 10.0f + 42.0f / 60.0f);
            timeStampAfter += (24.0f * 23 + 10.0f + 44.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 47 + 10.0f + 42.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 47 + 10.0f + 44.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<MWRender::MoonState::Phase>(7));
            EXPECT_EQ(afterState.mPhase, static_cast<MWRender::MoonState::Phase>(0));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(7));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<MWRender::MoonState::Phase>(0));
        }

        // OFFSETS

        TEST(MWWorldWeatherTest, secundaShouldApplyIncrementOffsetAfterFirstLoop)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 50.0f;

            // Days 8 and 32, 3:16
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 8 + 3.0f + 15.0f / 60.0f);
            timeStampAfter += (24.0f * 8 + 3.0f + 17.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 32 + 3.0f + 15.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 32 + 3.0f + 17.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_LE(beforeState.mMoonAlpha, 0.0f);
            EXPECT_GT(afterState.mMoonAlpha, 0.0f);
            EXPECT_LE(beforeStatePostLoop.mMoonAlpha, 0.0f);
            EXPECT_GT(afterStatePostLoop.mMoonAlpha, 0.0f);
        }

        TEST(MWWorldWeatherTest, moonWithLowIncrementShouldApplyIncrementOffsetAfterCycle)
        {
            float dailyIncrement = 0.9f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 0.0f;
            float fadeInFinish = 0.0f;
            float fadeOutStart = 0.0f;
            float fadeOutFinish = 0.0f;
            float axisOffset = 35.0f;

            // Days 7 and 31, 1:44
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 7 + 1.0f + 43.0f / 60.0f);
            timeStampAfter += (24.0f * 7 + 1.0f + 45.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 31 + 1.0f + 43.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 31 + 1.0f + 45.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_LE(beforeState.mMoonAlpha, 0.0f);
            EXPECT_GT(afterState.mMoonAlpha, 0.0f);
            EXPECT_LE(beforeStatePostLoop.mMoonAlpha, 0.0f);
            EXPECT_GT(afterStatePostLoop.mMoonAlpha, 0.0f);
        }

        TEST(MWWorldWeatherTest, masserShouldApplyIncrementOffsetAfterCycle)
        {
            float dailyIncrement = 1.0f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 0.0f;
            float fadeInFinish = 0.0f;
            float fadeOutStart = 0.0f;
            float fadeOutFinish = 0.0f;
            float axisOffset = 35.0f;

            // Days 4 and 28, 1:02
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 4 + 1.0f + 1.0f / 60.0f);
            timeStampAfter += (24.0f * 4 + 1.0f + 3.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 28 + 1.0f + 1.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 28 + 1.0f + 3.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_LE(beforeState.mMoonAlpha, 0.0f);
            EXPECT_GT(afterState.mMoonAlpha, 0.0f);
            EXPECT_LE(beforeStatePostLoop.mMoonAlpha, 0.0f);
            EXPECT_GT(afterStatePostLoop.mMoonAlpha, 0.0f);
        }

        TEST(MWWorldWeatherTest, secundaShouldApplyIncrementOffsetAfterCycle)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 0.0f;
            float fadeInFinish = 0.0f;
            float fadeOutStart = 0.0f;
            float fadeOutFinish = 0.0f;
            float axisOffset = 50.0f;

            // Days 3 and 27, 2:04
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 3 + 2.0f + 3.0f / 60.0f);
            timeStampAfter += (24.0f * 3 + 2.0f + 5.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 27 + 2.0f + 3.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 27 + 2.0f + 5.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_LE(beforeState.mMoonAlpha, 0.0f);
            EXPECT_GT(afterState.mMoonAlpha, 0.0f);
            EXPECT_LE(beforeStatePostLoop.mMoonAlpha, 0.0f);
            EXPECT_GT(afterStatePostLoop.mMoonAlpha, 0.0f);
        }

        TEST(MWWorldWeatherTest, moonWithIncreasedSpeedShouldApplyIncrementOffsetAfterCycle)
        {
            float dailyIncrement = 1.2f;
            float speed = 1.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 0.0f;
            float fadeInFinish = 0.0f;
            float fadeOutStart = 0.0f;
            float fadeOutFinish = 0.0f;
            float axisOffset = 50.0f;

            // Days 4 and 28, 1:13
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 4 + 1.0f + 12.0f / 60.0f);
            timeStampAfter += (24.0f * 4 + 1.0f + 14.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 28 + 1.0f + 12.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 28 + 1.0f + 14.0f / 60.0f);

            MWWorld::MoonModel moon = MWWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            MWRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            MWRender::MoonState afterState = moon.calculateState(timeStampAfter);
            MWRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            MWRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_LE(beforeState.mMoonAlpha, 0.0f);
            EXPECT_GT(afterState.mMoonAlpha, 0.0f);
            EXPECT_LE(beforeStatePostLoop.mMoonAlpha, 0.0f);
            EXPECT_GT(afterStatePostLoop.mMoonAlpha, 0.0f);
        }
    }
}
