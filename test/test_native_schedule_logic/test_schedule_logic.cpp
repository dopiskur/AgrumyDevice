// Roadmap #19/#95: native (host-based) tests for computeScheduleState() (roadmap #39's wall-clock
// schedule window) - no ESP32/Arduino dependency, runs via `pio test -e native`.
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

// ---- day-of-week mask filtering -------------------------------------------------------

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

// ---- start/duration window boundaries --------------------------------------------------

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

// ---- localSecondsOfDay's own valid range (0..86399) ------------------------------------

void test_LocalSecondsOfDayZero_WithinMidnightWindow_IsOn(void)
{
    TEST_ASSERT_TRUE(computeScheduleState(kSaturday, 0, 60, 6, 0));
}

void test_LocalSecondsOfDayLastSecond_WithinLateWindow_IsOn(void)
{
    // 86399 is the last valid second of a day (86400 itself belongs to the next day already).
    TEST_ASSERT_TRUE(computeScheduleState(kSaturday, 86340, 60, 6, 86399));
}

// ---- no midnight-wraparound support in v1 (server rejects start+duration > 86400 before it -----
// ---- ever reaches a device, so this function does not special-case it - pinning that here) -----

void test_WindowExtendingPastMidnight_DoesNotWrapToNextDay(void)
{
    // start=86000, duration=1000 -> nominal window [86000, 87000), which the server would in
    // practice never send (DeviceApiController.ScheduleWindowError rejects start+duration>86400)
    // - but if it ever did reach the device, this proves the firmware has no wraparound logic:
    // the tail end of "today" is honored (localSecondsOfDay can't exceed 86399 anyway)...
    TEST_ASSERT_TRUE(computeScheduleState(kAllDays, 86000, 1000, 3, 86399));
    // ...but the "spillover" into the next calendar day is NOT: 100 seconds after midnight on the
    // NEXT day is < 86000, so it falls outside [86000, 87000) even though a wrapping
    // implementation would have included it.
    TEST_ASSERT_FALSE(computeScheduleState(kAllDays, 86000, 1000, 4, 100));
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
    return UNITY_END();
}
