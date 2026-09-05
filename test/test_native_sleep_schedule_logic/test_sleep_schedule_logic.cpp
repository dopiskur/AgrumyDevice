#include <unity.h>
#include "../../src/Logic/SleepScheduleLogic.h"

void setUp(void) {}
void tearDown(void) {}

// Days-of-week mask: bit0=Sunday..bit6=Saturday.
const int EVERY_DAY = 0b1111111;

void test_Schedule_BeforeWindowToday_ReturnsSecondsUntilStart(void)
{
    // Window 09:00-10:00 (32400-36000), now is 08:00 (28800) on a scheduled day.
    TEST_ASSERT_EQUAL(3600, secondsUntilScheduleBoundary(EVERY_DAY, 32400, 3600, 3, 28800));
}

void test_Schedule_InsideWindowToday_ReturnsSecondsUntilEnd(void)
{
    // Window 09:00-10:00, now is 09:30 (34200) - 30 minutes left in the window.
    TEST_ASSERT_EQUAL(1800, secondsUntilScheduleBoundary(EVERY_DAY, 32400, 3600, 3, 34200));
}

void test_Schedule_ExactlyAtStart_ReturnsSecondsUntilEnd(void)
{
    // At the boundary itself, computeScheduleState already reports "on" (start is inclusive) - the next transition is the end, not another start.
    TEST_ASSERT_EQUAL(3600, secondsUntilScheduleBoundary(EVERY_DAY, 32400, 3600, 3, 32400));
}

void test_Schedule_AfterWindowToday_SkipsToNextScheduledDay(void)
{
    // Only Sunday (bit0) and Wednesday (bit3) scheduled; now is Wednesday (weekday 3), window already passed today - next occurrence is Sunday, 4 days away.
    int mask = 0b0001001;
    int candidate = secondsUntilScheduleBoundary(mask, 32400, 3600, 3, 50000);
    int expected = 4 * 86400 - 50000 + 32400;
    TEST_ASSERT_EQUAL(expected, candidate);
}

void test_Schedule_OnlyTodayScheduled_WindowPassed_WrapsToNextWeek(void)
{
    // Only today's bit (weekday 3) set, window already ended - must wrap to the same weekday next week (d=7), not report -1.
    int mask = 0b0001000;
    int candidate = secondsUntilScheduleBoundary(mask, 32400, 3600, 3, 50000);
    int expected = 7 * 86400 - 50000 + 32400;
    TEST_ASSERT_EQUAL(expected, candidate);
}

void test_Schedule_NoDaysSet_ReturnsMinusOne(void)
{
    TEST_ASSERT_EQUAL(-1, secondsUntilScheduleBoundary(0, 32400, 3600, 3, 28800));
}

void test_Schedule_TomorrowScheduled_ReturnsSecondsUntilTomorrowsStart(void)
{
    // Weekday 4 (Thursday) scheduled, now is Wednesday (weekday 3) at 50000s; tomorrow's start is (86400-50000)+32400 away.
    int mask = 1 << 4;
    int candidate = secondsUntilScheduleBoundary(mask, 32400, 3600, 3, 50000);
    int expected = 86400 - 50000 + 32400;
    TEST_ASSERT_EQUAL(expected, candidate);
}

void test_Interval_CurrentlyOn_ReturnsSecondsUntilOff(void)
{
    // interval=3600, intervalLength=600, epoch=100 -> on, 500s left until off.
    TEST_ASSERT_EQUAL(500, secondsUntilIntervalBoundary(3600, 600, (time_t)100));
}

void test_Interval_CurrentlyOff_ReturnsSecondsUntilNextCycle(void)
{
    // epoch=1000 -> positionInCycle=1000, off (>= 600), 2600s until the cycle wraps.
    TEST_ASSERT_EQUAL(2600, secondsUntilIntervalBoundary(3600, 600, (time_t)1000));
}

void test_Interval_ZeroOrNegativeInterval_ReturnsMinusOne(void)
{
    TEST_ASSERT_EQUAL(-1, secondsUntilIntervalBoundary(0, 600, (time_t)100));
    TEST_ASSERT_EQUAL(-1, secondsUntilIntervalBoundary(-5, 600, (time_t)100));
}

void test_ClampToSleepFloor_BelowFloor_RaisesToFloor(void)
{
    TEST_ASSERT_EQUAL(30, clampToSleepFloor(5, 30));
}

void test_ClampToSleepFloor_AtOrAboveFloor_Unchanged(void)
{
    TEST_ASSERT_EQUAL(30, clampToSleepFloor(30, 30));
    TEST_ASSERT_EQUAL(120, clampToSleepFloor(120, 30));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_Schedule_BeforeWindowToday_ReturnsSecondsUntilStart);
    RUN_TEST(test_Schedule_InsideWindowToday_ReturnsSecondsUntilEnd);
    RUN_TEST(test_Schedule_ExactlyAtStart_ReturnsSecondsUntilEnd);
    RUN_TEST(test_Schedule_AfterWindowToday_SkipsToNextScheduledDay);
    RUN_TEST(test_Schedule_OnlyTodayScheduled_WindowPassed_WrapsToNextWeek);
    RUN_TEST(test_Schedule_NoDaysSet_ReturnsMinusOne);
    RUN_TEST(test_Schedule_TomorrowScheduled_ReturnsSecondsUntilTomorrowsStart);
    RUN_TEST(test_Interval_CurrentlyOn_ReturnsSecondsUntilOff);
    RUN_TEST(test_Interval_CurrentlyOff_ReturnsSecondsUntilNextCycle);
    RUN_TEST(test_Interval_ZeroOrNegativeInterval_ReturnsMinusOne);
    RUN_TEST(test_ClampToSleepFloor_BelowFloor_RaisesToFloor);
    RUN_TEST(test_ClampToSleepFloor_AtOrAboveFloor_Unchanged);
    return UNITY_END();
}
