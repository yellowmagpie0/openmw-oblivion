#include <gtest/gtest.h>

#include <cmath>

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
