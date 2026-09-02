#include <gtest/gtest.h>

#include <cstring>
#include <limits>
#include <vector>

#include <components/esm4/aipackagedata.hpp>

namespace
{
    void putU32(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value)
    {
        for (unsigned i = 0; i < 4; ++i)
            data.at(offset + i) = static_cast<std::uint8_t>(value >> (i * 8));
    }

}

TEST(ESM4AIPackageData, KeepsTypeAndUnknownFlagBits)
{
    EXPECT_EQ(ESM4::packageTypeFromRaw(0), ESM4::AIPackageType::Find);
    EXPECT_EQ(ESM4::packageTypeFromRaw(11), ESM4::AIPackageType::CastMagic);
    EXPECT_FALSE(ESM4::packageTypeFromRaw(12));

    const ESM4::PackageFlags flags = ESM4::decodePackageFlags(
        static_cast<std::uint32_t>(ESM4::PackageFlag::AlwaysRun) | 0x80000000u);
    EXPECT_TRUE(flags.has(ESM4::PackageFlag::AlwaysRun));
    EXPECT_EQ(flags.mKnown, static_cast<std::uint32_t>(ESM4::PackageFlag::AlwaysRun));
    EXPECT_EQ(flags.mReserved, 0x80000000u);
}

TEST(ESM4AIPackageData, UsesTheNativeOblivionObjectTypeOrder)
{
    EXPECT_TRUE(ESM4::isKnownPackageObjectType(0));
    EXPECT_TRUE(ESM4::isKnownPackageObjectType(35));
    EXPECT_FALSE(ESM4::isKnownPackageObjectType(36));

    EXPECT_TRUE(ESM4::packageObjectTypeMatches(1, ESM::REC_ACTI4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(2, ESM::REC_APPA4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(3, ESM::REC_ARMO4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(4, ESM::REC_BOOK4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(5, ESM::REC_CLOT4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(6, ESM::REC_CONT4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(7, ESM::REC_DOOR4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(8, ESM::REC_INGR4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(9, ESM::REC_LIGH4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(10, ESM::REC_MISC4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(11, ESM::REC_FLOR4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(12, ESM::REC_FURN4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(13, ESM::REC_WEAP4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(14, ESM::REC_AMMO4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(15, ESM::REC_NPC_4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(16, ESM::REC_CREA4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(17, ESM::REC_SLGM4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(18, ESM::REC_KEYM4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(19, ESM::REC_ALCH4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(20, ESM::REC_ALCH4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(21, ESM::REC_ARMO4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(22, ESM::REC_CLOT4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(23, ESM::REC_WEAP4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(24, ESM::REC_WEAP4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(25, ESM::REC_WEAP4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(26, ESM::REC_SPEL4));
    EXPECT_TRUE(ESM4::packageObjectTypeMatches(35, ESM::REC_SPEL4));

    EXPECT_FALSE(ESM4::packageObjectTypeMatches(2, ESM::REC_ARMO4));
    EXPECT_FALSE(ESM4::packageObjectTypeMatches(38, ESM::REC_MISC4));
}

TEST(ESM4AIPackageData, HandlesScheduleWildcardsAndMidnightWrap)
{
    ESM4::PackageSchedule schedule;
    schedule.mStartHour = 22;
    schedule.mDuration = 4;
    const ESM4::CalendarInstant before{ 2024, 0, 1, 21.99 };
    const ESM4::CalendarInstant during{ 2024, 0, 2, 1.0 };
    const ESM4::CalendarInstant after{ 2024, 0, 2, 2.0 };
    EXPECT_FALSE(schedule.isEligible(before));
    EXPECT_TRUE(schedule.isEligible(during));
    EXPECT_FALSE(schedule.isEligible(after));

    schedule.mStartHour = -1;
    schedule.mDuration = 0;
    EXPECT_TRUE(schedule.isEligible({ 2024, 0, 10, 23.9 }));
    EXPECT_EQ(schedule.effectiveDurationHours(), 24.0);

    schedule.mMonth = 1;
    schedule.mDate = 31;
    EXPECT_FALSE(schedule.isValid());
    EXPECT_FALSE(schedule.isEligible({ 2024, 1, 29, 12.0 }));

    schedule = {};
    schedule.mStartHour = -1;
    schedule.mDuration = 4;
    EXPECT_TRUE(schedule.isEligible({ 2024, 0, 10, 0.1 }));
    EXPECT_TRUE(schedule.isEligible({ 2024, 0, 10, 23.9 }));

    EXPECT_EQ(ESM4::calendarDayOfWeek({ 2000, 0, 2, 0.0 }), 0); // Sundas
    EXPECT_EQ(ESM4::calendarDayOfWeek({ 2000, 0, 3, 0.0 }), 1); // Morndas
}

TEST(ESM4AIPackageData, MeasuresRemainingScheduleTimeAcrossMidnight)
{
    ESM4::PackageSchedule schedule;
    schedule.mStartHour = 22;
    schedule.mDuration = 4;
    const auto window = schedule.activeWindow({ 2024, 0, 2, 1.5 });
    ASSERT_TRUE(window);
    EXPECT_DOUBLE_EQ(ESM4::calendarHoursUntil({ 2024, 0, 2, 1.5 }, window->mEnd), 0.5);
    EXPECT_DOUBLE_EQ(ESM4::calendarHoursUntil(window->mStart, window->mEnd), 4.0);
}

TEST(ESM4AIPackageData, FindsMatchingStartDaysAcrossMonthAndYearBoundaries)
{
    ESM4::PackageSchedule schedule;
    schedule.mMonth = 11;
    schedule.mDate = 31;
    schedule.mStartHour = 23;
    schedule.mDuration = 4;
    EXPECT_TRUE(schedule.isEligible({ 2024, 0, 1, 1.0 }));
    const auto window = schedule.activeWindow({ 2024, 0, 1, 1.0 });
    ASSERT_TRUE(window);
    EXPECT_EQ(window->mStart, (ESM4::CalendarInstant{ 2023, 11, 31, 23.0 }));

    schedule = {};
    schedule.mMonth = 1;
    schedule.mDate = 29;
    schedule.mStartHour = 22;
    schedule.mDuration = 6;
    EXPECT_TRUE(schedule.isEligible({ 2024, 2, 1, 2.0 }));
    EXPECT_FALSE(schedule.isEligible({ 2023, 2, 1, 2.0 }));

    schedule = {};
    schedule.mStartHour = -1;
    schedule.mDuration = 48;
    EXPECT_TRUE(schedule.isEligible({ 2024, 0, 2, 12.0 }));
}

TEST(ESM4AIPackageData, DecodesCTDAAndCTDTParameterTypes)
{
    const auto resolver = [](ESM::FormId raw) {
        return ESM::FormKey::content("Oblivion.esm", raw.mIndex);
    };
    std::vector<std::uint8_t> ctda(24);
    ctda[0] = 0x67; // OR, run-on-target, use-global, >=
    putU32(ctda, 4, 0x01001234);
    putU32(ctda, 8, 60); // both parameters are FormIDs
    putU32(ctda, 12, 0x01000001);
    putU32(ctda, 16, 0x01000002);
    putU32(ctda, 20, 0xfeedbeef);

    const ESM4::PackageCondition value = ESM4::decodePackageCondition(ctda, resolver);
    EXPECT_EQ(value.mEncodedSize, 24);
    EXPECT_EQ(value.mOperator, ESM4::ConditionOperator::GreaterOrEqual);
    EXPECT_TRUE(value.mOr);
    EXPECT_TRUE(value.mRunOnTarget);
    EXPECT_TRUE(value.mUseGlobal);
    EXPECT_TRUE(value.mParameter1.mIsFormId);
    EXPECT_TRUE(value.mParameter2.mIsFormId);
    EXPECT_EQ(value.mUnknownTail, 0xfeedbeef);
    EXPECT_EQ(value.mRunOn, ESM4::ConditionRunOn::Unknown);
    EXPECT_EQ(value.mRunOnReferenceKey, ESM::FormKey());

    putU32(ctda, 20, 2); // run on explicit reference
    const ESM4::PackageCondition referenceValue = ESM4::decodePackageCondition(ctda, resolver);
    EXPECT_EQ(referenceValue.mRunOn, ESM4::ConditionRunOn::Reference);

    std::vector<std::uint8_t> ctdt(20);
    ctdt[8] = 14; // GetActorValue: numeric parameters
    putU32(ctdt, 12, 17);
    putU32(ctdt, 16, 0);
    const ESM4::PackageCondition shortValue = ESM4::decodePackageCondition(ctdt, resolver);
    EXPECT_EQ(shortValue.mEncodedSize, 20);
    EXPECT_FALSE(shortValue.mParameter1.mIsFormId);
    EXPECT_EQ(shortValue.mParameter1.mNumber, 17);
    EXPECT_EQ(shortValue.mRunOn, ESM4::ConditionRunOn::Subject);
}

TEST(ESM4AIPackageData, ComparisonRejectsUnknownOperatorsAndNonFiniteValues)
{
    EXPECT_TRUE(ESM4::conditionIsTrue(2, 1, ESM4::ConditionOperator::Greater));
    EXPECT_FALSE(ESM4::conditionIsTrue(2, 1, ESM4::ConditionOperator::Less));
    EXPECT_FALSE(ESM4::conditionIsTrue(2, 1, ESM4::ConditionOperator::Unknown));
    EXPECT_FALSE(ESM4::conditionIsTrue(std::numeric_limits<double>::quiet_NaN(), 1,
        ESM4::ConditionOperator::Equal));
}
