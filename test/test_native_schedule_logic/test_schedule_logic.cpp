#include <unity.h>
#include "../../src/Logic/RelayLogic.h"

void setUp(void) {}
void tearDown(void) {}

// Bit numbering matches C's tm_wday directly: 0=Sunday .. 6=Saturday.
static const int kSunday = 1 << 0;
static const int kMonday = 1 << 1;
static const int kWednesday = 1 << 3;
static const int kSaturday = 1 << 6;
static const int kWeekdays = kMonday | (1 << 2) | kWednesday | (1 << 4) | (1 << 5); // Mon-Fri
static const int kAllDays = 0b1111111;

void test_DayBitSet_WithinWindow_IsOn(void)
{
    // Wednesday (bit 3), window [21600, 64800) i.e. 06:00-18:00, checked at noon.
    TEST_ASSERT_TRUE(computeScheduleState(kWednesday, 21600, 43200, 3, 43200));
}

void test_DayBitNotSet_IsOff_EvenIfTimeMatches(void)
{
    // Same window/time as above, but only Monday is scheduled - today (Wednesday) is not.
    TEST_ASSERT_FALSE(computeScheduleState(kMonday, 21600, 43200, 3, 43200));
}

void test_WeekdaysMask_SaturdayExcluded(void)
{
    TEST_ASSERT_FALSE(computeScheduleState(kWeekdays, 0, 86400, 6, 100));
}

void test_WeekdaysMask_WednesdayIncluded(void)
{
    TEST_ASSERT_TRUE(computeScheduleState(kWeekdays, 0, 86400, 3, 100));
}

void test_AllDaysMask_EveryWeekdayIncluded(void)
{
    for (int weekday = 0; weekday <= 6; weekday++)
    {
        TEST_ASSERT_TRUE_MESSAGE(computeScheduleState(kAllDays, 0, 86400, weekday, 100), "expected every day of week to be scheduled");
    }
}

void test_ExactlyAtStart_IsOn(void)
{
    // [start, start+duration) is inclusive of start.
    TEST_ASSERT_TRUE(computeScheduleState(kSunday, 21600, 3600, 0, 21600));
}

void test_OneSecondBeforeStart_IsOff(void)
{
    TEST_ASSERT_FALSE(computeScheduleState(kSunday, 21600, 3600, 0, 21599));
}

void test_OneSecondBeforeWindowEnd_IsOn(void)
{
    TEST_ASSERT_TRUE(computeScheduleState(kSunday, 21600, 3600, 0, 25199)); // 21600+3600-1
}

void test_ExactlyAtWindowEnd_IsOff(void)
{
    // [start, start+duration) excludes the end instant itself.
    TEST_ASSERT_FALSE(computeScheduleState(kSunday, 21600, 3600, 0, 25200)); // 21600+3600
}

void test_LocalSecondsOfDayZero_WithinMidnightWindow_IsOn(void)
{
    TEST_ASSERT_TRUE(computeScheduleState(kSaturday, 0, 60, 6, 0));
}

void test_LocalSecondsOfDayLastSecond_WithinLateWindow_IsOn(void)
{
    // 86399 is the last valid second of a day (86400 itself belongs to the next day already).
    TEST_ASSERT_TRUE(computeScheduleState(kSaturday, 86340, 60, 6, 86399));
}

void test_WindowExtendingPastMidnight_DoesNotWrapToNextDay(void)
{
    // start=86000, duration=1000 -> nominal window [86000, 87000). Proves the firmware has no wraparound logic: the tail end of "today" is honored...
    TEST_ASSERT_TRUE(computeScheduleState(kAllDays, 86000, 1000, 3, 86399));
    // ...but spillover into the next calendar day is NOT: 100s after midnight is < 86000, so it falls outside [86000, 87000) even though a wrapping implementation would include it.
    TEST_ASSERT_FALSE(computeScheduleState(kAllDays, 86000, 1000, 4, 100));
}

void test_AnySchedule_ZeroSlots_IsOff(void)
{
    ScheduleWindow slots[1] = {};
    TEST_ASSERT_FALSE(computeAnyScheduleState(slots, 0, 3, 43200));
}

void test_AnySchedule_SingleSlot_SameAsComputeScheduleState(void)
{
    ScheduleWindow slots[1] = { { kWednesday, 21600, 43200 } };
    TEST_ASSERT_TRUE(computeAnyScheduleState(slots, 1, 3, 43200));
    TEST_ASSERT_FALSE(computeAnyScheduleState(slots, 1, 3, 90000)); // outside the one window
}

void test_AnySchedule_TwoNonOverlappingSlots_OnInEitherWindow(void)
{
    // 06:00-06:30 and 14:00-14:15, every day.
    ScheduleWindow slots[2] = { { kAllDays, 21600, 1800 }, { kAllDays, 50400, 900 } };
    TEST_ASSERT_TRUE_MESSAGE(computeAnyScheduleState(slots, 2, 2, 21700), "expected on within first window");
    TEST_ASSERT_TRUE_MESSAGE(computeAnyScheduleState(slots, 2, 2, 50500), "expected on within second window");
    TEST_ASSERT_FALSE_MESSAGE(computeAnyScheduleState(slots, 2, 2, 30000), "expected off between the two windows");
}

void test_AnySchedule_OneEnabledOneWrongDay_OnlyMatchingSlotCounts(void)
{
    // Same time window, but only the Wednesday slot's day matches a Wednesday check.
    ScheduleWindow slots[2] = { { kMonday, 21600, 1800 }, { kWednesday, 21600, 1800 } };
    TEST_ASSERT_TRUE(computeAnyScheduleState(slots, 2, 3, 21700)); // Wednesday
    TEST_ASSERT_FALSE(computeAnyScheduleState(slots, 2, 5, 21700)); // Friday - neither slot's day matches
}

void test_AnySchedule_OverlappingSlots_StillJustOn(void)
{
    // Overlap is explicitly allowed - OR of two true results is still just true, not some special double-on state.
    ScheduleWindow slots[2] = { { kAllDays, 21600, 3600 }, { kAllDays, 23400, 3600 } }; // overlap [23400,25200)
    TEST_ASSERT_TRUE(computeAnyScheduleState(slots, 2, 0, 24000));
}

void test_AnySchedule_OnlyFirstCountSlotsConsidered(void)
{
    // A slot beyond `count` (e.g. a stale array entry never cleared) must be ignored.
    ScheduleWindow slots[2] = { { kAllDays, 0, 60 }, { kAllDays, 50000, 60 } };
    TEST_ASSERT_TRUE(computeAnyScheduleState(slots, 2, 0, 50010));
    TEST_ASSERT_FALSE_MESSAGE(computeAnyScheduleState(slots, 1, 0, 50010), "count=1 must not look at slots[1]");
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_DayBitSet_WithinWindow_IsOn);
    RUN_TEST(test_DayBitNotSet_IsOff_EvenIfTimeMatches);
    RUN_TEST(test_WeekdaysMask_SaturdayExcluded);
    RUN_TEST(test_WeekdaysMask_WednesdayIncluded);
    RUN_TEST(test_AllDaysMask_EveryWeekdayIncluded);
    RUN_TEST(test_ExactlyAtStart_IsOn);
    RUN_TEST(test_OneSecondBeforeStart_IsOff);
    RUN_TEST(test_OneSecondBeforeWindowEnd_IsOn);
    RUN_TEST(test_ExactlyAtWindowEnd_IsOff);
    RUN_TEST(test_LocalSecondsOfDayZero_WithinMidnightWindow_IsOn);
    RUN_TEST(test_LocalSecondsOfDayLastSecond_WithinLateWindow_IsOn);
    RUN_TEST(test_WindowExtendingPastMidnight_DoesNotWrapToNextDay);
    RUN_TEST(test_AnySchedule_ZeroSlots_IsOff);
    RUN_TEST(test_AnySchedule_SingleSlot_SameAsComputeScheduleState);
    RUN_TEST(test_AnySchedule_TwoNonOverlappingSlots_OnInEitherWindow);
    RUN_TEST(test_AnySchedule_OneEnabledOneWrongDay_OnlyMatchingSlotCounts);
    RUN_TEST(test_AnySchedule_OverlappingSlots_StillJustOn);
    RUN_TEST(test_AnySchedule_OnlyFirstCountSlotsConsidered);
    return UNITY_END();
}
