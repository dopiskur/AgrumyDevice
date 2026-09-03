#include <unity.h>
#include "../../src/Logic/RelayLogic.h"

void setUp(void) {}
void tearDown(void) {}

void test_MidWindow_IsOn(void)
{
    // interval=3600 (1h cycle), intervalLength=600 (10 min on): epoch=100 is well inside the ON window.
    TEST_ASSERT_TRUE(computeIntervalState(3600, 600, 100));
}

void test_WellPastWindow_IsOff(void)
{
    TEST_ASSERT_FALSE(computeIntervalState(3600, 600, 2000));
}

void test_EpochZero_IsOn(void)
{
    // positionInCycle = 0 % interval = 0, which is < intervalLength (as long as intervalLength > 0).
    TEST_ASSERT_TRUE(computeIntervalState(3600, 600, 0));
}

void test_ExactCycleBoundary_WrapsToStartOfNextCycle_IsOn(void)
{
    // epoch == interval exactly: positionInCycle wraps to 0, same as epoch=0 - a fresh cycle starts.
    TEST_ASSERT_TRUE(computeIntervalState(3600, 600, 3600));
}

void test_MultipleCyclesIn_SameBehaviorAsFirstCycle(void)
{
    // epoch = 100 cycles in + 100s - must behave identically to epoch=100 (pure modulo).
    TEST_ASSERT_TRUE(computeIntervalState(3600, 600, (time_t)3600 * 100 + 100));
}

void test_OneSecondBeforeIntervalLength_IsOn(void)
{
    TEST_ASSERT_TRUE(computeIntervalState(3600, 600, 599));
}

void test_ExactlyAtIntervalLength_IsOff(void)
{
    // positionInCycle < intervalLength is a strict less-than - exactly at the boundary is OFF.
    TEST_ASSERT_FALSE(computeIntervalState(3600, 600, 600));
}

void test_IntervalZero_ReturnsFalse_NoDivideByZero(void)
{
    TEST_ASSERT_FALSE(computeIntervalState(0, 600, 100));
}

void test_IntervalNegative_ReturnsFalse(void)
{
    TEST_ASSERT_FALSE(computeIntervalState(-5, 600, 100));
}

void test_IntervalLengthZero_AlwaysOff(void)
{
    // 0 < 0 is never true - a zero-length "on" window never actually turns anything on.
    TEST_ASSERT_FALSE(computeIntervalState(3600, 0, 0));
    TEST_ASSERT_FALSE(computeIntervalState(3600, 0, 3599));
}

void test_IntervalLengthEqualsInterval_AlwaysOn(void)
{
    // positionInCycle is always in [0, interval), which is always < intervalLength == interval.
    TEST_ASSERT_TRUE(computeIntervalState(3600, 3600, 0));
    TEST_ASSERT_TRUE(computeIntervalState(3600, 3600, 3599));
}

void test_LargeEpoch_NearUnsignedLongWraparound_StillDeterministic(void)
{
    // 4,000,000,000 is close to UINT32_MAX - proves the modulo arithmetic behaves the same as plain (epoch % interval) even this close to the `unsigned long` width on the real ESP32 target.
    time_t epoch = 4000000000UL;
    unsigned long expectedPosition = (unsigned long)epoch % 3600UL;
    bool expected = expectedPosition < 600UL;
    TEST_ASSERT_EQUAL(expected, computeIntervalState(3600, 600, epoch));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_MidWindow_IsOn);
    RUN_TEST(test_WellPastWindow_IsOff);
    RUN_TEST(test_EpochZero_IsOn);
    RUN_TEST(test_ExactCycleBoundary_WrapsToStartOfNextCycle_IsOn);
    RUN_TEST(test_MultipleCyclesIn_SameBehaviorAsFirstCycle);
    RUN_TEST(test_OneSecondBeforeIntervalLength_IsOn);
    RUN_TEST(test_ExactlyAtIntervalLength_IsOff);
    RUN_TEST(test_IntervalZero_ReturnsFalse_NoDivideByZero);
    RUN_TEST(test_IntervalNegative_ReturnsFalse);
    RUN_TEST(test_IntervalLengthZero_AlwaysOff);
    RUN_TEST(test_IntervalLengthEqualsInterval_AlwaysOn);
    RUN_TEST(test_LargeEpoch_NearUnsignedLongWraparound_StillDeterministic);
    return UNITY_END();
}
