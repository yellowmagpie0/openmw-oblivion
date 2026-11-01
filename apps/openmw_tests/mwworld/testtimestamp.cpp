#include <gtest/gtest.h>

#include <cmath>

#include "apps/openmw/mwworld/timestamp.hpp"
#include "apps/openmw/mwworld/datetimemanager.hpp"

namespace MWWorld
{
    namespace
    {
        TEST(MWWorldTimeStampTest, RuntimeSwapPreservesRenderingClocksAndExchangesPauseTags)
        {
            DateTimeManager original, incoming;
            original.setSimulationTime(3.5);
            incoming.setSimulationTime(9.5);
            original.setRenderingSimulationTime(10);
            incoming.setRenderingSimulationTime(20);
            original.pause("outgoing");
            incoming.pause("incoming");
            original.swapRuntime(incoming);
            EXPECT_EQ(original.getSimulationTime(), 9.5);
            EXPECT_EQ(incoming.getSimulationTime(), 3.5);
            EXPECT_EQ(original.getRenderingSimulationTime(), 10);
            EXPECT_EQ(incoming.getRenderingSimulationTime(), 20);
            EXPECT_EQ(original.getPausedTags().size(), 1u);
            EXPECT_TRUE(original.getPausedTags().contains("incoming"));
            EXPECT_EQ(incoming.getPausedTags().size(), 1u);
            EXPECT_TRUE(incoming.getPausedTags().contains("outgoing"));
        }

        TEST(MWWorldTimeStampTest, operatorPlusShouldNotChangeTimeStampForZero)
        {
            TimeStamp timeStamp;
            timeStamp += 0;
            EXPECT_EQ(timeStamp.getDay(), 0);
            EXPECT_EQ(timeStamp.getHour(), 0);
        }

        TEST(MWWorldTimeStampTest, operatorPlusShouldProperlyHandleDoubleValuesCloseTo24)
        {
            TimeStamp timeStamp;
            timeStamp += std::nextafter(24.0, 0.0);
            EXPECT_EQ(timeStamp.getDay(), 0);
            EXPECT_LT(timeStamp.getHour(), 24);
            EXPECT_FLOAT_EQ(timeStamp.getHour(), 24);
        }

        TEST(MWWorldTimeStampTest, operatorPlusShouldProperlyHandleFloatValuesCloseTo24)
        {
            TimeStamp timeStamp;
            timeStamp += std::nextafter(24.0f, 0.0f);
            EXPECT_EQ(timeStamp.getDay(), 0);
            EXPECT_LT(timeStamp.getHour(), 24);
            EXPECT_FLOAT_EQ(timeStamp.getHour(), 24);
        }

        TEST(MWWorldTimeStampTest, operatorPlusShouldAddDaysForEach24Hours)
        {
            TimeStamp timeStamp;
            timeStamp += 24.0 * 42;
            EXPECT_EQ(timeStamp.getDay(), 42);
            EXPECT_EQ(timeStamp.getHour(), 0);
        }

        TEST(MWWorldTimeStampTest, operatorPlusShouldAddDaysForEach24HoursAndSetRemainderToHours)
        {
            TimeStamp timeStamp;
            timeStamp += 24.0 * 42 + 13.0;
            EXPECT_EQ(timeStamp.getDay(), 42);
            EXPECT_EQ(timeStamp.getHour(), 13);
        }

        TEST(MWWorldTimeStampTest, operatorPlusShouldAccumulateExistingValue)
        {
            TimeStamp timeStamp(13, 42);
            timeStamp += 24.0 * 2 + 17.0;
            EXPECT_EQ(timeStamp.getDay(), 45);
            EXPECT_EQ(timeStamp.getHour(), 6);
        }

        TEST(MWWorldTimeStampTest, operatorPlusShouldThrowExceptionForNegativeValue)
        {
            TimeStamp timeStamp(13, 42);
            EXPECT_THROW(timeStamp += -1, std::runtime_error);
        }
    }
}
