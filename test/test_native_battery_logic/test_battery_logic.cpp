// Roadmap #12: native (host-based) tests for BatteryLogic.h - resistor-divider math and the
// approximate LiPo voltage->percent curve. No ESP32/Arduino dependency, runs via `pio test -e native`.
#include <unity.h>
#include "../../src/Logic/BatteryLogic.h"

void setUp(void) {}
void tearDown(void) {}

// ---- computeDividerBatteryVoltage ---------------------------------------------------------

void test_Divider_1to1_DoublesMeasuredVoltage(void)
{
    // Standard 100k/100k preset: V_battery = V_measured * (100k+100k)/100k = V_measured * 2.
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 3.7, computeDividerBatteryVoltage(1.85, 100000.0, 100000.0));
}

void test_Divider_UnevenRatio_ScalesCorrectly(void)
{
    // R1=200k, R2=100k -> factor (200k+100k)/100k = 3.
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 4.2, computeDividerBatteryVoltage(1.4, 200000.0, 100000.0));
}

void test_Divider_ZeroR2_ReturnsZero_NotDivideByZero(void)
{
    // Defensive: an unconfigured (0/0) sensor row must not crash the firmware.
    TEST_ASSERT_EQUAL_DOUBLE(0.0, computeDividerBatteryVoltage(1.85, 100000.0, 0.0));
}

// ---- computeBatteryPercentFromVoltage -----------------------------------------------------

void test_Percent_AtOrBelowEmpty_ReturnsZero(void)
{
    TEST_ASSERT_EQUAL_INT(0, computeBatteryPercentFromVoltage(3.00));
    TEST_ASSERT_EQUAL_INT(0, computeBatteryPercentFromVoltage(2.5)); // below the curve entirely
}

void test_Percent_AtOrAboveFull_ReturnsHundred(void)
{
    TEST_ASSERT_EQUAL_INT(100, computeBatteryPercentFromVoltage(4.20));
    TEST_ASSERT_EQUAL_INT(100, computeBatteryPercentFromVoltage(4.35)); // above the curve entirely
}

void test_Percent_AtKnownReferencePoints_MatchesExactly(void)
{
    TEST_ASSERT_EQUAL_INT(20, computeBatteryPercentFromVoltage(3.60));
    TEST_ASSERT_EQUAL_INT(50, computeBatteryPercentFromVoltage(3.75));
    TEST_ASSERT_EQUAL_INT(80, computeBatteryPercentFromVoltage(4.00));
}

void test_Percent_BetweenReferencePoints_InterpolatesLinearly(void)
{
    // Midpoint of the 3.70(40%)-3.75(50%) segment -> 45%.
    TEST_ASSERT_EQUAL_INT(45, computeBatteryPercentFromVoltage(3.725));
}

void test_Percent_SteepLowSegment_ReflectsFrontLoadedCurve(void)
{
    // 3.30(5%)-3.50(10%) is a much flatter volts-per-percent slope than 3.50(10%)-3.60(20%) -
    // the curve is deliberately not a single straight line end to end.
    TEST_ASSERT_EQUAL_INT(15, computeBatteryPercentFromVoltage(3.55));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_Divider_1to1_DoublesMeasuredVoltage);
    RUN_TEST(test_Divider_UnevenRatio_ScalesCorrectly);
    RUN_TEST(test_Divider_ZeroR2_ReturnsZero_NotDivideByZero);
    RUN_TEST(test_Percent_AtOrBelowEmpty_ReturnsZero);
    RUN_TEST(test_Percent_AtOrAboveFull_ReturnsHundred);
    RUN_TEST(test_Percent_AtKnownReferencePoints_MatchesExactly);
    RUN_TEST(test_Percent_BetweenReferencePoints_InterpolatesLinearly);
    RUN_TEST(test_Percent_SteepLowSegment_ReflectsFrontLoadedCurve);
    return UNITY_END();
}
