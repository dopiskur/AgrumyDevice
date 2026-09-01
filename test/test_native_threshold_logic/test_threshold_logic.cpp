// Roadmap #19/#95: native (host-based) tests for computeThresholdState() (roadmap #10's dead-zone
// hysteresis latch) - no ESP32/Arduino dependency, runs via `pio test -e native`.
#include <unity.h>
#include "../../src/Logic/RelayLogic.h"

void setUp(void) {}
void tearDown(void) {}

// ---- normal direction (turnsOnAboveThreshold=false): heating/light/waterPump ------------
// Turns ON when reading drops BELOW threshold, turns OFF once it climbs back to/above
// threshold+hysteresis. E.g. heating: threshold=tempLow=18, hysteresis=1 -> on below 18, off at >=19.

void test_Normal_Off_ReadingBelowThreshold_TurnsOn(void)
{
    TEST_ASSERT_TRUE(computeThresholdState(false, 17.0, 18.0, 1.0, false));
}

void test_Normal_Off_ReadingAtThreshold_StaysOff(void)
{
    // strictly-less-than: reading == threshold does not count as "below".
    TEST_ASSERT_FALSE(computeThresholdState(false, 18.0, 18.0, 1.0, false));
}

void test_Normal_Off_ReadingInDeadZone_StaysOff(void)
{
    // 18.5 is between threshold (18) and threshold+hysteresis (19) - not below threshold, so an
    // already-off relay must not spuriously turn on from inside the dead zone.
    TEST_ASSERT_FALSE(computeThresholdState(false, 18.5, 18.0, 1.0, false));
}

void test_Normal_On_ReadingStillInDeadZone_StaysOn(void)
{
    // The core dead-zone guarantee: once on, a reading that has climbed back above threshold but
    // NOT yet reached threshold+hysteresis must NOT turn off - this is what prevents chattering.
    TEST_ASSERT_TRUE(computeThresholdState(true, 18.5, 18.0, 1.0, false));
}

void test_Normal_On_ReadingJustBelowUpperBound_StaysOn(void)
{
    TEST_ASSERT_TRUE(computeThresholdState(true, 18.999, 18.0, 1.0, false));
}

void test_Normal_On_ReadingAtUpperBound_TurnsOff(void)
{
    // reading >= threshold+hysteresis is the OFF condition - inclusive of the boundary itself.
    TEST_ASSERT_FALSE(computeThresholdState(true, 19.0, 18.0, 1.0, false));
}

void test_Normal_On_ReadingWellAboveUpperBound_TurnsOff(void)
{
    TEST_ASSERT_FALSE(computeThresholdState(true, 25.0, 18.0, 1.0, false));
}

// ---- inverted direction (turnsOnAboveThreshold=true): ventilation only -------------------
// Turns ON when reading climbs ABOVE threshold (humidHigh), turns OFF once it drops back to/below
// threshold-hysteresis - exhausting excess humidity, the opposite polarity from the other three.

void test_Inverted_Off_ReadingAboveThreshold_TurnsOn(void)
{
    TEST_ASSERT_TRUE(computeThresholdState(false, 85.0, 80.0, 5.0, true));
}

void test_Inverted_Off_ReadingAtThreshold_StaysOff(void)
{
    TEST_ASSERT_FALSE(computeThresholdState(false, 80.0, 80.0, 5.0, true));
}

void test_Inverted_Off_ReadingInDeadZone_StaysOff(void)
{
    // 77 is between threshold-hysteresis (75) and threshold (80) - not above threshold.
    TEST_ASSERT_FALSE(computeThresholdState(false, 77.0, 80.0, 5.0, true));
}

void test_Inverted_On_ReadingStillInDeadZone_StaysOn(void)
{
    TEST_ASSERT_TRUE(computeThresholdState(true, 77.0, 80.0, 5.0, true));
}

void test_Inverted_On_ReadingAtLowerBound_TurnsOff(void)
{
    // reading <= threshold-hysteresis is the OFF condition - inclusive of the boundary itself.
    TEST_ASSERT_FALSE(computeThresholdState(true, 75.0, 80.0, 5.0, true));
}

void test_Inverted_On_ReadingWellBelowLowerBound_TurnsOff(void)
{
    TEST_ASSERT_FALSE(computeThresholdState(true, 50.0, 80.0, 5.0, true));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_Normal_Off_ReadingBelowThreshold_TurnsOn);
    RUN_TEST(test_Normal_Off_ReadingAtThreshold_StaysOff);
    RUN_TEST(test_Normal_Off_ReadingInDeadZone_StaysOff);
    RUN_TEST(test_Normal_On_ReadingStillInDeadZone_StaysOn);
    RUN_TEST(test_Normal_On_ReadingJustBelowUpperBound_StaysOn);
    RUN_TEST(test_Normal_On_ReadingAtUpperBound_TurnsOff);
    RUN_TEST(test_Normal_On_ReadingWellAboveUpperBound_TurnsOff);
    RUN_TEST(test_Inverted_Off_ReadingAboveThreshold_TurnsOn);
    RUN_TEST(test_Inverted_Off_ReadingAtThreshold_StaysOff);
    RUN_TEST(test_Inverted_Off_ReadingInDeadZone_StaysOff);
    RUN_TEST(test_Inverted_On_ReadingStillInDeadZone_StaysOn);
    RUN_TEST(test_Inverted_On_ReadingAtLowerBound_TurnsOff);
    RUN_TEST(test_Inverted_On_ReadingWellBelowLowerBound_TurnsOff);
    return UNITY_END();
}
