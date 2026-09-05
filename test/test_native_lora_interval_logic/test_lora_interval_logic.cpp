#include <unity.h>
#include "../../src/Logic/LoRaIntervalLogic.h"

void setUp(void) {}
void tearDown(void) {}

void test_AnchorPoints_MatchExactly(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(30.0, loRaIntervalSecondsForSpreadingFactor(7));
    TEST_ASSERT_EQUAL_DOUBLE(120.0, loRaIntervalSecondsForSpreadingFactor(9));
    TEST_ASSERT_EQUAL_DOUBLE(300.0, loRaIntervalSecondsForSpreadingFactor(12));
}

void test_BelowLowestAnchor_ClampsToSf7(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(30.0, loRaIntervalSecondsForSpreadingFactor(6));
    TEST_ASSERT_EQUAL_DOUBLE(30.0, loRaIntervalSecondsForSpreadingFactor(0));
    TEST_ASSERT_EQUAL_DOUBLE(30.0, loRaIntervalSecondsForSpreadingFactor(-5));
}

void test_AboveHighestAnchor_ClampsToSf12(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(300.0, loRaIntervalSecondsForSpreadingFactor(13));
    TEST_ASSERT_EQUAL_DOUBLE(300.0, loRaIntervalSecondsForSpreadingFactor(20));
}

void test_Interpolated_StrictlyIncreasesWithSf(void)
{
    for (int sf = 8; sf <= 11; sf++)
    {
        double below = loRaIntervalSecondsForSpreadingFactor(sf - 1);
        double at = loRaIntervalSecondsForSpreadingFactor(sf);
        double above = loRaIntervalSecondsForSpreadingFactor(sf + 1);
        TEST_ASSERT_TRUE(below < at);
        TEST_ASSERT_TRUE(at < above);
    }
}

void test_BatteryPowered_DoublesTheInterval(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(60.0, loRaIntervalSecondsForNode(7, true));
    TEST_ASSERT_EQUAL_DOUBLE(30.0, loRaIntervalSecondsForNode(7, false));
}

void test_Eu868DataRate_MapsToSpreadingFactor(void)
{
    TEST_ASSERT_EQUAL(12, loRaSpreadingFactorForEu868DataRate(0));
    TEST_ASSERT_EQUAL(11, loRaSpreadingFactorForEu868DataRate(1));
    TEST_ASSERT_EQUAL(10, loRaSpreadingFactorForEu868DataRate(2));
    TEST_ASSERT_EQUAL(9, loRaSpreadingFactorForEu868DataRate(3));
    TEST_ASSERT_EQUAL(8, loRaSpreadingFactorForEu868DataRate(4));
    TEST_ASSERT_EQUAL(7, loRaSpreadingFactorForEu868DataRate(5));
}

void test_Eu868DataRate_NonSfRatesFallBackToSf7(void)
{
    TEST_ASSERT_EQUAL(7, loRaSpreadingFactorForEu868DataRate(6));
    TEST_ASSERT_EQUAL(7, loRaSpreadingFactorForEu868DataRate(7));
    TEST_ASSERT_EQUAL(7, loRaSpreadingFactorForEu868DataRate(-1));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_AnchorPoints_MatchExactly);
    RUN_TEST(test_BelowLowestAnchor_ClampsToSf7);
    RUN_TEST(test_AboveHighestAnchor_ClampsToSf12);
    RUN_TEST(test_Interpolated_StrictlyIncreasesWithSf);
    RUN_TEST(test_BatteryPowered_DoublesTheInterval);
    RUN_TEST(test_Eu868DataRate_MapsToSpreadingFactor);
    RUN_TEST(test_Eu868DataRate_NonSfRatesFallBackToSf7);
    return UNITY_END();
}
